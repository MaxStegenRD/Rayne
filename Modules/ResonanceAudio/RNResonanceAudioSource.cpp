//
//  RNResonanceAudioSource.cpp
//  Rayne-ResonanceAudio
//
//  Copyright 2023 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNResonanceAudioSource.h"
#include "RNResonanceAudioSampler.h"
#include "RNResonanceAudioWorld.h"

#include <api/resonance_audio_api.h>

namespace RN
{
	static constexpr uint32 kFadeSamples = 32;

	static vraudio::DistanceRolloffModel MapRolloffModel(ResonanceAudioSource::DistanceRolloffModel model)
	{
		switch(model)
		{
			case ResonanceAudioSource::DistanceRolloffModel::Logarithmic: return vraudio::DistanceRolloffModel::kLogarithmic;
			case ResonanceAudioSource::DistanceRolloffModel::Linear: return vraudio::DistanceRolloffModel::kLinear;
			case ResonanceAudioSource::DistanceRolloffModel::None: return vraudio::DistanceRolloffModel::kNone;
		}
		return vraudio::DistanceRolloffModel::kLinear;
	}

	RNDefineMeta(ResonanceAudioSource, SceneNode)

	ResonanceAudioSource::ResonanceAudioSource(AudioAsset *asset, bool wantsIndirectSound, bool isPositional) :
		_channel(0),
		_sampler(new ResonanceAudioSampler(asset)),
		_sourceID(vraudio::ResonanceAudioApi::kInvalidSourceId),
		_wantsIndirectSound(wantsIndirectSound),
		_isPositional(isPositional),
		_isPlaying(false),
		_isRepeating(false),
		_isSelfdestructing(false),
		_volume(1.0f),
		_pitch(1.0f),
		_minMaxRange(RN::Vector2(0.2f, 200.0f)),
		_rolloffModel(DistanceRolloffModel::Logarithmic),
		_currentTime(0.0f),
		_fadeSamples(0),
		_pendingSeekTime(0.0),
		_pendingAsset(nullptr),
		_controlBits(0),
		_finalAction(PendingAction::None),
		_cachedHasAsset(asset != nullptr),
		_cachedTotalTime(asset ? _sampler->GetTotalTime() : 0.0),
		_cachedIsPlaying(false),
		_cachedCurrentTime(0.0)
	{
		RN_ASSERT(ResonanceAudioWorld::_instance, "You need to create a ResonanceAudioWorld before creating audio sources!");

		ResonanceAudioWorld::_instance->AddAudioSource(this);

		if(_isPositional)
		{
			//TODO: Make quality adjustable
			_sourceID = ResonanceAudioWorld::_instance->_audioAPI->CreateSoundObjectSource(vraudio::RenderingMode::kBinauralHighQuality);
			ResonanceAudioWorld::_instance->_audioAPI->SetSourceDistanceModel(_sourceID, MapRolloffModel(_rolloffModel), _minMaxRange.x, _minMaxRange.y);
		}
	}

	ResonanceAudioSource::~ResonanceAudioSource()
	{
		AudioAsset *pendingAsset = _pendingAsset.exchange(nullptr, std::memory_order_acq_rel);
		SafeRelease(pendingAsset);
		ResonanceAudioWorld::_instance->RemoveAudioSource(this);
		if(_isPositional) ResonanceAudioWorld::_instance->_audioAPI->DestroySource(_sourceID);
		_sampler->Release();
	}

	void ResonanceAudioSource::SetAudioAsset(AudioAsset *asset)
	{
		AudioAsset* retained = asset ? SafeRetain(asset) : nullptr;
		AudioAsset* old = _pendingAsset.exchange(retained, std::memory_order_acq_rel);
		SafeRelease(old);

		SubmitPendingAction(PendingAction::Asset);
	}

	void ResonanceAudioSource::SetRepeat(bool repeat)
	{
		_isRepeating.store(repeat, std::memory_order_relaxed);
	}

	void ResonanceAudioSource::SetChannel(uint8 channel)
	{
		_channel.store(channel, std::memory_order_relaxed);
	}

	void ResonanceAudioSource::SetCurrentDistanceAttenuationValue(float attentuation)
	{
		if(!_isPositional) return;
		RN_DEBUG_ASSERT(_rolloffModel == DistanceRolloffModel::None, "Distance attenuation value is only supported for none rolloff model");

		ResonanceAudioWorld::_instance->_audioAPI->SetSourceDistanceAttenuation(_sourceID, attentuation);
	}

	void ResonanceAudioSource::SetPitch(float pitch)
	{
		_pitch.store(pitch, std::memory_order_relaxed);
	}

	void ResonanceAudioSource::SetVolume(float volume)
	{
		_volume.store(volume, std::memory_order_relaxed);
		if(!_isPositional) return;
		ResonanceAudioWorld::_instance->_audioAPI->SetSourceVolume(_sourceID, volume);
	}

	void ResonanceAudioSource::SetRange(RN::Vector2 minMaxRange)
	{
		if(!_isPositional) return;
		if(_minMaxRange == minMaxRange) return;
		
		_minMaxRange = minMaxRange;

		// Clamp to sane positives to avoid API errors
		const float minDistance = std::max(0.001f, _minMaxRange.x);
		const float maxDistance = std::max(minDistance, _minMaxRange.y);
		_minMaxRange = RN::Vector2(minDistance, maxDistance);

		ResonanceAudioWorld::_instance->_audioAPI->SetSourceDistanceModel(_sourceID, MapRolloffModel(_rolloffModel), minDistance, maxDistance);
	}

	void ResonanceAudioSource::SetSelfdestruct(bool selfdestruct)
	{
		_isSelfdestructing.store(selfdestruct, std::memory_order_relaxed);
	}

	void ResonanceAudioSource::SetRolloffModel(DistanceRolloffModel rolloffModel)
	{
		if(!_isPositional) return;
		if(_rolloffModel == rolloffModel) return;

		_rolloffModel = rolloffModel;

		const float minDistance = std::max(0.001f, _minMaxRange.x);
		const float maxDistance = std::max(minDistance, _minMaxRange.y);
		ResonanceAudioWorld::_instance->_audioAPI->SetSourceDistanceModel(_sourceID, MapRolloffModel(_rolloffModel), minDistance, maxDistance);
	}


	void ResonanceAudioSource::Play()
	{
		SubmitPendingAction(PendingAction::Play);
	}

	void ResonanceAudioSource::Stop()
	{
		SubmitPendingAction(PendingAction::Stop);
	}

	void ResonanceAudioSource::Pause()
	{
		SubmitPendingAction(PendingAction::Pause);
	}

	void ResonanceAudioSource::Seek(double time)
	{
		_pendingSeekTime.store(time, std::memory_order_relaxed);
		SubmitPendingAction(PendingAction::Seek);
	}

	bool ResonanceAudioSource::HasEnded() const
	{
		bool cachedHasAsset = _cachedHasAsset.load(std::memory_order_relaxed);
		if(!cachedHasAsset) return true;
		if(_isRepeating.load(std::memory_order_relaxed)) return false;
		double cachedTotalTime = _cachedTotalTime.load(std::memory_order_relaxed);
		double cachedCurrentTime = _cachedCurrentTime.load(std::memory_order_relaxed);
		return (cachedCurrentTime >= cachedTotalTime);
	}

	bool ResonanceAudioSource::Update(double frameLength, uint32 sampleCount, float **outputBuffer, uint8 channelCount)
	{
		AudioAsset *asset = _sampler->GetAsset();
		if(!asset || !_isPlaying)
		{
			*outputBuffer = nullptr;
			return false;
		}

		if(_sampler->GetAsset()->GetType() == AudioAsset::Type::Ringbuffer)
		{
			//Buffer for audio data to play
			uint32 bytesPerSecond = _sampler->GetAsset()->GetSampleRate() * _sampler->GetAsset()->GetBytesPerSample();
			uint32 assetFrameSamples = std::round(frameLength * bytesPerSecond);
			if(_sampler->GetAsset()->GetBufferedSize() < assetFrameSamples)
			{
				*outputBuffer = nullptr;
				return false;
			}
			else
			{
				//Skip samples if data is written faster than played
				uint32 maxBufferedLength = assetFrameSamples * 20;
				if(_sampler->GetAsset()->GetBufferedSize() > maxBufferedLength)
				{
					uint32 skipBytes = _sampler->GetAsset()->GetBufferedSize() - assetFrameSamples;
					double skipTime = skipBytes / static_cast<double>(bytesPerSecond);
					_currentTime += skipTime;
					_sampler->GetAsset()->PopData(nullptr, skipBytes);
				}

				_sampler->GetAsset()->PopData(nullptr, assetFrameSamples);
			}
		}

		double sampleLength = frameLength / static_cast<double>(sampleCount);
		double localTime = _currentTime;
		bool isRepeating = _isRepeating.load(std::memory_order_relaxed);
		uint8 channel = _channel.load(std::memory_order_relaxed);
		float pitch = _pitch.load(std::memory_order_relaxed);
		float volume = _isPositional ? 1.0f : _volume.load(std::memory_order_relaxed);

		for(int i = 0; i < sampleCount; i++)
		{
			float gain = volume;

			if(_fadeSamples > 0)
			{
				gain *= 1.0f - (static_cast<float>(_fadeSamples - 1) / static_cast<float>(kFadeSamples));
				_fadeSamples -= 1;
			}
			else if(_fadeSamples < 0)
			{
				gain *= static_cast<float>(-_fadeSamples) / static_cast<float>(kFadeSamples);
				_fadeSamples += 1;
			}

			for(int j = 0; j < channelCount; j++)
			{
				float value = _isPlaying ? _sampler->GetSample(localTime, j + channel, isRepeating) : 0.0f;
				ResonanceAudioWorld::_instance->_sharedFrameData[i * channelCount + j] = value * gain;
			}
			if(_isPlaying) localTime += sampleLength * pitch;

			if(_fadeSamples == 0)
			{
				if(ProcessPendingActions())
				{
					localTime = _currentTime;
				}
			}
		}

		_currentTime = localTime;
		_cachedCurrentTime.store(_currentTime, std::memory_order_relaxed);

		*outputBuffer = ResonanceAudioWorld::_instance->_sharedFrameData;
		return true;
	}

	void ResonanceAudioSource::Update()
	{
		if(!_isPlaying)
		{
			ProcessPendingActions();
		}
		
		if(_isPositional && _isPlaying)
		{
			const uint32 frameSize = ResonanceAudioWorld::_instance->_audioSystem->_frameSize;
			const uint32 sampleRate = ResonanceAudioWorld::_instance->_audioSystem->_sampleRate;

			//For none positional sources this happens in the audio handling callback
			float *newBuffer;
			Update(static_cast<double>(frameSize) / static_cast<double>(sampleRate), frameSize, &newBuffer, 1);
			ResonanceAudioWorld::_instance->_audioAPI->SetInterleavedBuffer(_sourceID, newBuffer, 1, frameSize);
		}

		if(HasEnded())
		{
			_isPlaying = false;
			_cachedIsPlaying.store(false, std::memory_order_relaxed);

			if(_isSelfdestructing.load(std::memory_order_relaxed) && GetSceneInfo() && GetSceneInfo()->GetScene())
			{
				GetSceneInfo()->GetScene()->RemoveNode(const_cast<ResonanceAudioSource *>(this));
			}
		}
	}

	void ResonanceAudioSource::DidUpdate(SceneNode::ChangeSet changeSet)
	{
		SceneNode::DidUpdate(changeSet);

		if(changeSet & SceneNode::ChangeSet::Position || changeSet & SceneNode::ChangeSet::Attachments)
		{
			RN::Vector3 position = GetWorldPosition();
			RN::Quaternion rotation = GetWorldRotation();
			ResonanceAudioWorld::_instance->_audioAPI->SetSourcePosition(_sourceID, position.x, position.y, position.z);
			ResonanceAudioWorld::_instance->_audioAPI->SetSourceRotation(_sourceID, rotation.x, rotation.y, rotation.z, rotation.w);
		}
	}

	void ResonanceAudioSource::SubmitPendingAction(PendingAction action)
	{
		switch(action)
		{
			case PendingAction::Seek:
			{
				uint32_t bitsToSet = static_cast<uint32_t>(ControlBits::kWantSeek) | static_cast<uint32_t>(ControlBits::kWantFadeOut) | static_cast<uint32_t>(ControlBits::kWantFadeIn);
				_controlBits.fetch_or(bitsToSet, std::memory_order_release);
				break;
			}

			case PendingAction::Asset:
			{
				// Asset swap while playing should be guarded by fades
				uint32_t bitsToSet = static_cast<uint32_t>(ControlBits::kWantFadeOut) | static_cast<uint32_t>(ControlBits::kWantAssetChange) | static_cast<uint32_t>(ControlBits::kWantFadeIn);
				_controlBits.fetch_or(bitsToSet, std::memory_order_release);
				break;
			}

			case PendingAction::Stop:
			{
				_finalAction.store(action, std::memory_order_release);
				_pendingSeekTime.store(0.0, std::memory_order_relaxed);
				_controlBits.fetch_or(static_cast<uint32_t>(ControlBits::kWantFadeOut) | static_cast<uint32_t>(ControlBits::kWantSeek), std::memory_order_release);
				_controlBits.fetch_and(~static_cast<uint32_t>(ControlBits::kWantFadeIn), std::memory_order_release);
				break;
			}

			case PendingAction::Pause:
			{
				_finalAction.store(action, std::memory_order_release);
				_controlBits.fetch_or(static_cast<uint32_t>(ControlBits::kWantFadeOut), std::memory_order_release);
				_controlBits.fetch_and(~static_cast<uint32_t>(ControlBits::kWantFadeIn), std::memory_order_release);
				break;
			}

			case PendingAction::Play:
			{
				_finalAction.store(action, std::memory_order_release);
				_controlBits.fetch_or(static_cast<uint32_t>(ControlBits::kWantFadeIn), std::memory_order_release);
				// No need to force FadeOut for Play.
				break;
			}

			default:
				break;
		}
	}

	bool ResonanceAudioSource::ProcessPendingActions()
	{
		bool isSeeking = false;
		uint32_t controlBits = _controlBits.exchange(0, std::memory_order_acq_rel);

		if(controlBits & static_cast<uint32_t>(ControlBits::kWantFadeOut))
		{
			uint32_t remainingBits = controlBits & ~static_cast<uint32_t>(ControlBits::kWantFadeOut);
			if(remainingBits) _controlBits.fetch_or(remainingBits, std::memory_order_release);
			_fadeSamples = -static_cast<int32>(kFadeSamples);
			return isSeeking;
		}

		if(controlBits & static_cast<uint32_t>(ControlBits::kWantAssetChange))
		{
			AudioAsset *pendingAsset = _pendingAsset.exchange(nullptr, std::memory_order_acq_rel);
			if(pendingAsset != _sampler->GetAsset())
			{
				_sampler->SetAudioAsset(pendingAsset);
				// Update cached values after asset change
				_cachedHasAsset.store(_sampler->GetAsset() != nullptr, std::memory_order_relaxed);
				_cachedTotalTime.store(_sampler->GetTotalTime(), std::memory_order_relaxed);

				_currentTime = 0.0;
				_cachedCurrentTime.store(0.0, std::memory_order_relaxed);
				isSeeking = true;
			}
			SafeRelease(pendingAsset);
		}

		if(controlBits & static_cast<uint32_t>(ControlBits::kWantSeek) && !isSeeking)
		{
			_currentTime = _pendingSeekTime.load(std::memory_order_relaxed);
			_cachedCurrentTime.store(_currentTime, std::memory_order_relaxed);
			isSeeking = true;
		}

		PendingAction finalAction = _finalAction.exchange(PendingAction::None, std::memory_order_acq_rel);
		switch(finalAction)
		{
			case PendingAction::Stop:
			case PendingAction::Pause:
				_isPlaying = false;
				_cachedIsPlaying.store(false, std::memory_order_relaxed);
				controlBits &= ~static_cast<uint32_t>(ControlBits::kWantFadeIn);
				break;

			case PendingAction::Play:
				if(_sampler->GetAsset() && _currentTime >= _sampler->GetTotalTime())
				{
					_currentTime = 0.0f;
					_cachedCurrentTime.store(0.0, std::memory_order_relaxed);
				}
				_isPlaying = true;
				_cachedIsPlaying.store(true, std::memory_order_relaxed);
				break;

			default:
				break;
		}

		if(controlBits & static_cast<uint32_t>(ControlBits::kWantFadeIn))
		{
			if(_isPlaying)
			{
				_fadeSamples = static_cast<int32>(kFadeSamples);
			}
		}

		return isSeeking;
	}
} // namespace RN
