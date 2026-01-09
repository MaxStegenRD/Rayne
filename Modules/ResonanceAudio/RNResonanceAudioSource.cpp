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
			case ResonanceAudioSource::DistanceRolloffModel::Inverse: return vraudio::DistanceRolloffModel::kInverse;
			case ResonanceAudioSource::DistanceRolloffModel::None: return vraudio::DistanceRolloffModel::kNone;
		}
		return vraudio::DistanceRolloffModel::kLinear;
	}

	RNDefineMeta(ResonanceAudioSource, SceneNode)

	void ResonanceAudioSource::Update(float delta)
	{
		SceneNode::Update(delta);

		auto ResetDoppler = [&] {
			_dopplerPitchMultiplier.store(1.0f, std::memory_order_relaxed);
			_dopplerInitialized = false;
		};

		if(!_isPositional)
			return;

		ResonanceAudioWorld *world = ResonanceAudioWorld::GetInstance();
		if(!world || delta <= 0.0f || !IsPlaying())
		{
			ResetDoppler();
			return;
		}

		const ResonanceAudioWorld::ListenerState listenerState = world->GetListenerState();
		if(!listenerState.isValid)
		{
			ResetDoppler();
			return;
		}

		const float dopplerFactor = world->_dopplerFactor;
		const float speedOfSound = world->_dopplerSpeedOfSound;
		if(dopplerFactor <= 0.0f || speedOfSound <= 0.0f)
		{
			ResetDoppler();
			return;
		}

		const Vector3 listenerPosition = listenerState.position;
		const Vector3 listenerVelocity = listenerState.velocity;
		const Vector3 sourcePosition = GetWorldPosition();

		if(!_dopplerInitialized)
		{
			_dopplerOldPosition = sourcePosition;
			_dopplerVelocity = Vector3(0.0f, 0.0f, 0.0f);
			_dopplerInitialized = true;
			_dopplerPitchMultiplier.store(1.0f, std::memory_order_relaxed);
			return;
		}

		const float smoothingOld = std::clamp(world->_dopplerVelocitySmoothing, 0.0f, 0.999f);
		Vector3 rawVelocity = (sourcePosition - _dopplerOldPosition) / delta;
		_dopplerOldPosition = sourcePosition;
		_dopplerVelocity = _dopplerVelocity * smoothingOld + rawVelocity * (1.0f - smoothingOld);

		Vector3 dir = listenerPosition - sourcePosition; // source -> listener
		const float dist = dir.GetLength();
		if(dist <= k::EpsilonFloat)
		{
			_dopplerPitchMultiplier.store(1.0f, std::memory_order_relaxed);
			return;
		}
		dir = dir / dist;

		// Project velocities onto the source->listener direction.
		const float vls = listenerVelocity.GetDotProduct(dir);
		const float vss = _dopplerVelocity.GetDotProduct(dir);

		// OpenAL 1.1-style ratio: (c - factor*v_l) / (c - factor*v_s)
		// Keep strictly positive to avoid negative/NaN pitch factors.
		const float kMinDenom = 0.01f;
		float num = std::max(kMinDenom, speedOfSound - dopplerFactor * vls);
		float den = std::max(kMinDenom, speedOfSound - dopplerFactor * vss);
		float shift = num / den;

		// Prevent extreme pitch explosions.
		shift = std::clamp(shift, 0.125f, 8.0f);
		_dopplerPitchMultiplier.store(shift, std::memory_order_relaxed);
	}

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
		_dopplerPitchMultiplier(1.0f),
		_dopplerOldPosition(Vector3()),
		_dopplerVelocity(Vector3()),
		_dopplerInitialized(false),
		_minMaxRange(RN::Vector2(0.2f, 200.0f)),
		_rolloffModel(DistanceRolloffModel::Logarithmic),
		_currentTime(0.0f),
		_fadeSamples(0),
		_pendingSeekTime(0.0),
		_pendingAsset(nullptr),
		_controlBits(0),
		_finalAction(PendingAction::None),
		_pendingActionsWrite(&_pendingActionsBuffer[0]),
		_cachedHasAsset(asset != nullptr),
		_cachedTotalTime(asset ? _sampler->GetTotalTime() : 0.0),
		_cachedIsPlaying(false),
		_cachedCurrentTime(0.0)
	{
		// Ring buffers should always be repeating (streaming sources that never "end")
		if(asset && asset->GetType() == AudioAsset::Type::Ringbuffer)
		{
			_isRepeating.store(true, std::memory_order_relaxed);
		}

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
		ProcessPendingActionsQueue(); //Only needs to happen once per frame
		ProcessPendingActions(); //Needs to happen once per sample, but also here to actually be able to change assets while playing

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
		// Do not apply Doppler to ringbuffer-backed sources (typically streaming/voice).
		if(_isPositional && asset && asset->GetType() != AudioAsset::Type::Ringbuffer)
		{
			pitch *= _dopplerPitchMultiplier.load(std::memory_order_relaxed);
		}
		float volume = _isPositional ? 1.0f : _volume.load(std::memory_order_relaxed);
		bool isMonoAsset = _sampler->GetAsset()->GetChannels() == 1;

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
				int sampleChannel = isMonoAsset ? 0 : j;
				float value = _isPlaying ? _sampler->GetSample(localTime, sampleChannel + channel, isRepeating) : 0.0f;
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
			ProcessPendingActionsQueue();
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

		if(changeSet & SceneNode::ChangeSet::Position || changeSet & SceneNode::ChangeSet::Parent || changeSet & SceneNode::ChangeSet::World || changeSet & SceneNode::ChangeSet::Attachments)
		{
			RN::Vector3 position = GetWorldPosition();
			RN::Quaternion rotation = GetWorldRotation();
			ResonanceAudioWorld::_instance->_audioAPI->SetSourcePosition(_sourceID, position.x, position.y, position.z);
			ResonanceAudioWorld::_instance->_audioAPI->SetSourceRotation(_sourceID, rotation.x, rotation.y, rotation.z, rotation.w);
		}
	}

	void ResonanceAudioSource::SubmitPendingAction(PendingAction action)
	{
		std::vector<PendingAction> *writeBuffer = _pendingActionsWrite.load(std::memory_order_acquire);
		if(writeBuffer) writeBuffer->push_back(action);
	}

	void ResonanceAudioSource::ProcessPendingActionsQueue()
	{
		// Swap write buffer pointer to get current buffer for processing
		std::vector<PendingAction> *writeBuffer = _pendingActionsWrite.load(std::memory_order_acquire);
		if(!writeBuffer) return;
		
		std::vector<PendingAction> *readBuffer = _pendingActionsWrite.exchange(
			writeBuffer == &_pendingActionsBuffer[0] ? &_pendingActionsBuffer[1] : &_pendingActionsBuffer[0],
			std::memory_order_acq_rel);
		if(!readBuffer) return;
		
		// Process each action and set control bits/final action based on current state
		if(!readBuffer->empty())
		{
			for(PendingAction action : *readBuffer)
			{
				switch(action)
				{
					case PendingAction::Seek:
					{
						_controlBits |= static_cast<uint32_t>(ControlBits::kWantSeek);
						if(_isPlaying) _controlBits |= static_cast<uint32_t>(ControlBits::kWantFadeOut) | static_cast<uint32_t>(ControlBits::kWantFadeIn);
						break;
					}

					case PendingAction::Asset:
					{
						// Asset swap while playing should be guarded by fades
						_controlBits |= static_cast<uint32_t>(ControlBits::kWantAssetChange);
						if(_isPlaying && _sampler->GetAsset() != _pendingAsset.load(std::memory_order_relaxed)) _controlBits |= static_cast<uint32_t>(ControlBits::kWantFadeOut) | static_cast<uint32_t>(ControlBits::kWantFadeIn);
						break;
					}

					case PendingAction::Stop:
					{
						if(_currentTime > 0.0)
						{
							_pendingSeekTime.store(0.0, std::memory_order_relaxed);
							_controlBits |= static_cast<uint32_t>(ControlBits::kWantSeek);
						}
						
						if(_isPlaying)
						{
							_finalAction = action;
							_controlBits |= static_cast<uint32_t>(ControlBits::kWantFadeOut);
						}

						_controlBits &= ~static_cast<uint32_t>(ControlBits::kWantFadeIn);
						break;
					}

					case PendingAction::Pause:
					{
						if(_isPlaying)
						{
							_finalAction = action;
							_controlBits |= static_cast<uint32_t>(ControlBits::kWantFadeOut);
						}
						_controlBits &= ~static_cast<uint32_t>(ControlBits::kWantFadeIn);
						break;
					}

					case PendingAction::Play:
					{
						_finalAction = action;
						_controlBits |= static_cast<uint32_t>(ControlBits::kWantFadeIn);
						break;
					}

					default:
						break;
				}
			}
			
			// Clear the read buffer for next time
			readBuffer->clear();
		}
	}

	bool ResonanceAudioSource::ProcessPendingActions()
	{
		// Now process the control bits as before
		bool isSeeking = false;

		if(_controlBits & static_cast<uint32_t>(ControlBits::kWantFadeOut) && _isPlaying)
		{
			_fadeSamples = -static_cast<int32>(kFadeSamples);
			_controlBits &= ~static_cast<uint32_t>(ControlBits::kWantFadeOut);
			return isSeeking;
		}

		if(_controlBits & static_cast<uint32_t>(ControlBits::kWantAssetChange))
		{
			AudioAsset *pendingAsset = _pendingAsset.exchange(nullptr, std::memory_order_acq_rel);
			if(pendingAsset != _sampler->GetAsset())
			{
				_sampler->SetAudioAsset(pendingAsset);
				// Update cached values after asset change
				_cachedHasAsset.store(_sampler->GetAsset() != nullptr, std::memory_order_relaxed);
				_cachedTotalTime.store(_sampler->GetTotalTime(), std::memory_order_relaxed);

				// Ring buffers should always be repeating (streaming sources that never "end")
				if(pendingAsset && pendingAsset->GetType() == AudioAsset::Type::Ringbuffer)
				{
					_isRepeating.store(true, std::memory_order_relaxed);
				}

				_currentTime = 0.0;
				_cachedCurrentTime.store(0.0, std::memory_order_relaxed);
				isSeeking = true;
			}
			SafeRelease(pendingAsset);
			_controlBits &= ~static_cast<uint32_t>(ControlBits::kWantAssetChange);
		}

		if(_controlBits & static_cast<uint32_t>(ControlBits::kWantSeek) && !isSeeking)
		{
			_currentTime = _pendingSeekTime.load(std::memory_order_relaxed);
			_cachedCurrentTime.store(_currentTime, std::memory_order_relaxed);
			isSeeking = true;
			_controlBits &= ~static_cast<uint32_t>(ControlBits::kWantSeek);
		}

		switch(_finalAction)
		{
			case PendingAction::Stop:
			case PendingAction::Pause:
				_isPlaying = false;
				_cachedIsPlaying.store(false, std::memory_order_relaxed);
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
		_finalAction = PendingAction::None;

		if(_controlBits & static_cast<uint32_t>(ControlBits::kWantFadeIn))
		{
			if(_isPlaying)
			{
				_fadeSamples = static_cast<int32>(kFadeSamples);
			}

			_controlBits &= ~static_cast<uint32_t>(ControlBits::kWantFadeIn);
		}

		return isSeeking;
	}
} // namespace RN
