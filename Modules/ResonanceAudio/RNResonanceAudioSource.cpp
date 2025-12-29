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
		_hasTimeOfFlight(true),
		_hasReverb(true),
		_volume(1.0f),
		_pitch(1.0f),
		_minMaxRange(RN::Vector2(0.2f, 200.0f)),
		_rolloffModel(DistanceRolloffModel::Logarithmic),
		_currentTime(0.0f),
		_fadeSamples(0),
		_pendingSeekTime(0.0),
		_pendingAsset(nullptr),
		_wantsAssetChange(false),
		_wantsFadeIn(false),
		_wantsFadeOut(false),
		_wantsSeek(false),
		_finalAction(PendingAction::None),
		_cachedHasAsset(asset != nullptr),
		_cachedTotalTime(asset ? _sampler->GetTotalTime() : 0.0)
	{
		RN_ASSERT(ResonanceAudioWorld::_instance, "You need to create a ResonanceAudioWorld before creating audio sources!");

		ResonanceAudioWorld::_instance->AddAudioSource(this);

		if(_isPositional)
		{
			//TODO: Make quality adjustable
			_sourceID = ResonanceAudioWorld::_instance->_audioAPI->CreateSoundObjectSource(vraudio::RenderingMode::kBinauralHighQuality);
			ResonanceAudioWorld::_instance->_audioAPI->SetSourceDistanceModel(_sourceID, MapRolloffModel(_rolloffModel), 1.0f, 20.0f);
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
		if(!_isPlaying)
		{
			_wantsAssetChange.store(false, std::memory_order_release);
			AudioAsset* old = _pendingAsset.exchange(nullptr, std::memory_order_acq_rel);
			SafeRelease(old);

			_sampler->SetAudioAsset(asset);
			_currentTime = 0.0f;

			// Update cached values immediately when not playing
			_cachedHasAsset.store(asset != nullptr, std::memory_order_release);
			_cachedTotalTime.store(_sampler->GetTotalTime(), std::memory_order_release);
			return;
		}

		AudioAsset* retained = asset ? SafeRetain(asset) : nullptr;
		AudioAsset* old = _pendingAsset.exchange(retained, std::memory_order_acq_rel);
		SafeRelease(old);
		_wantsAssetChange.store(true, std::memory_order_release);

		SubmitPendingAction(PendingAction::Asset);
	}

	void ResonanceAudioSource::SetRepeat(bool repeat)
	{
		_sampler->SetRepeat(repeat);
		_isRepeating = repeat;
	}

	void ResonanceAudioSource::SetChannel(uint8 channel)
	{
		_channel = channel;
	}

	void ResonanceAudioSource::SetCurrentDistanceAttenuationValue(float attentuation)
	{
		if(!_isPositional) return;
		RN_DEBUG_ASSERT(_rolloffModel == DistanceRolloffModel::None, "Distance attenuation value is only supported for none rolloff model");

		ResonanceAudioWorld::_instance->_audioAPI->SetSourceDistanceAttenuation(_sourceID, attentuation);
	}

	void ResonanceAudioSource::SetPitch(float pitch)
	{
		_pitch = pitch;
	}

	void ResonanceAudioSource::SetVolume(float volume)
	{
		_volume = volume;
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
		_isSelfdestructing = selfdestruct;
	}

	void ResonanceAudioSource::SetTimeOfFlight(bool tof)
	{
		_hasTimeOfFlight = tof;
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
		_isPlaying = true;

		bool cachedHasAsset = _cachedHasAsset.load(std::memory_order_acquire);
		double cachedTotalTime = _cachedTotalTime.load(std::memory_order_acquire);
		if(cachedHasAsset && _currentTime >= cachedTotalTime)
		{
			_currentTime = 0.0f;
		}
	}

	void ResonanceAudioSource::Stop()
	{

		if(!_isPlaying)
		{
			_currentTime = 0.0;
			return;
		}

		SubmitPendingAction(PendingAction::Stop);
	}

	void ResonanceAudioSource::Pause()
	{
		SubmitPendingAction(PendingAction::Pause);
	}

	void ResonanceAudioSource::Seek(double time)
	{
		_pendingSeekTime = time;
		if(!_isPlaying)
		{
			_currentTime = time;
			return;
		}

		SubmitPendingAction(PendingAction::Seek);
	}

	bool ResonanceAudioSource::HasEnded() const
	{
		bool cachedHasAsset = _cachedHasAsset.load(std::memory_order_acquire);
		if(!cachedHasAsset) return true;
		if(_isRepeating) return false;
		double cachedTotalTime = _cachedTotalTime.load(std::memory_order_acquire);
		return (_currentTime >= cachedTotalTime);
	}

	void ResonanceAudioSource::Update(double frameLength, uint32 sampleCount, float **outputBuffer, uint8 channelCount)
	{
		AudioAsset *asset = _sampler->GetAsset();
		if(!asset)
		{
			*outputBuffer = nullptr;
			return;
		}

		if(_sampler->GetAsset()->GetType() == AudioAsset::Type::Ringbuffer)
		{
			//Buffer for audio data to play
			uint32 bytesPerSecond = _sampler->GetAsset()->GetSampleRate() * _sampler->GetAsset()->GetBytesPerSample();
			uint32 assetFrameSamples = std::round(frameLength * bytesPerSecond);
			if(_sampler->GetAsset()->GetBufferedSize() < assetFrameSamples)
			{
				*outputBuffer = nullptr;
				return;
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

		for(int i = 0; i < sampleCount; i++)
		{
			float gain = 1.0f;

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
				float value = _isPlaying ? _sampler->GetSample(localTime, j + _channel) : 0.0f;
				ResonanceAudioWorld::_instance->_sharedFrameData[i * channelCount + j] = value * gain;
			}
			if(_isPlaying) localTime += sampleLength * _pitch;

			if(_fadeSamples == 0)
			{
				if(ProcessPendingActions())
				{
					localTime = _currentTime;
				}
			}
		}

		_currentTime = localTime;

		*outputBuffer = ResonanceAudioWorld::_instance->_sharedFrameData;
	}

	void ResonanceAudioSource::Update()
	{
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

			if(_isSelfdestructing && GetSceneInfo() && GetSceneInfo()->GetScene())
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
				_wantsSeek = true;
				_wantsFadeOut = true;
				if(_finalAction != PendingAction::Stop && _finalAction != PendingAction::Pause) _wantsFadeIn = true;
				break;

			case PendingAction::Asset:
				// Asset swap while playing should be guarded by fades
				_wantsFadeOut = true;
				_wantsSeek = true;
				_pendingSeekTime = 0.0;
				if(_finalAction != PendingAction::Stop && _finalAction != PendingAction::Pause) _wantsFadeIn = true;
				break;

			case PendingAction::Stop:
				_finalAction = action;
				_wantsFadeOut = true;
				_wantsFadeIn = false;
				_wantsSeek = true;
				_pendingSeekTime = 0.0;
				break;

			case PendingAction::Pause:
				_finalAction  = action;
				_wantsFadeOut = true;
				_wantsFadeIn  = false;
				break;

			case PendingAction::Play:
				_finalAction = action;
				_wantsFadeIn = true;
				// No need to force FadeOut for Play.
				break;

			default:
				break;
		}
	}

	bool ResonanceAudioSource::ProcessPendingActions()
	{
		bool isSeeking = false;

		if(_wantsFadeOut)
		{
			_wantsFadeOut = false;
			_fadeSamples = -static_cast<int32>(kFadeSamples);
			return isSeeking;
		}

		if(_wantsAssetChange.exchange(false, std::memory_order_acq_rel))
		{
			AudioAsset *pendingAsset = _pendingAsset.exchange(nullptr, std::memory_order_acq_rel);
			_sampler->SetAudioAsset(pendingAsset);
			// Update cached values after asset change
			_cachedHasAsset.store(_sampler->GetAsset() != nullptr, std::memory_order_release);
			_cachedTotalTime.store(_sampler->GetTotalTime(), std::memory_order_release);
			SafeRelease(pendingAsset);
		}

		if(_wantsSeek)
		{
			_wantsSeek = false;
			_currentTime = _pendingSeekTime;
			isSeeking = true;
		}

		if(_finalAction != PendingAction::None)
		{
			const PendingAction action = _finalAction;
			_finalAction = PendingAction::None;

			switch(action)
			{
				case PendingAction::Stop:
					_isPlaying = false;
					break;

				case PendingAction::Pause:
					_isPlaying = false;
					break;

				case PendingAction::Play:
					_isPlaying = true;
					break;

				default:
					break;
			}
		}

		if(_wantsFadeIn)
		{
			_wantsFadeIn = false;

			if(_isPlaying)
			{
				_fadeSamples = static_cast<int32>(kFadeSamples);
			}
			return isSeeking;
		}

		return isSeeking;
	}
} // namespace RN
