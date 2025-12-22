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
		_currentTime(0.0f)
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
		ResonanceAudioWorld::_instance->RemoveAudioSource(this);
		if(_isPositional) ResonanceAudioWorld::_instance->_audioAPI->DestroySource(_sourceID);
		_sampler->Release();
	}

	void ResonanceAudioSource::SetAudioAsset(AudioAsset *asset)
	{
		// Avoid resetting playback time when the asset is unchanged (common for streaming ringbuffers like voice chat)
		if(_sampler->GetAsset() == asset) return;

		_sampler->SetAudioAsset(asset);
		_currentTime = 0.0f;
	}

	void ResonanceAudioSource::SetRepeat(bool repeat)
	{
		_sampler->SetRepeat(repeat);
		_isRepeating = repeat;
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
		_isPlaying = true;
		if(_sampler->GetAsset() && _currentTime >= _sampler->GetTotalTime())
		{
			_currentTime = 0.0f;
		}
	}

	void ResonanceAudioSource::Stop()
	{
		_isPlaying = false;
		_currentTime = 0.0;
	}

	void ResonanceAudioSource::Pause()
	{
		_isPlaying = false;
	}

	void ResonanceAudioSource::Seek(double time)
	{
		_currentTime = time;
	}

	bool ResonanceAudioSource::HasEnded() const
	{
		if(!_sampler->GetAsset()) return true;
		if(_isRepeating) return false;
		return (_currentTime >= _sampler->GetTotalTime());
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
			uint32 assetFrameSamples = std::round(frameLength * _sampler->GetAsset()->GetSampleRate() * _sampler->GetAsset()->GetBytesPerSample());
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
					double skipTime = skipBytes / _sampler->GetAsset()->GetBytesPerSample() / static_cast<double>(_sampler->GetAsset()->GetSampleRate());
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
			for(int j = 0; j < channelCount; j++)
			{
				ResonanceAudioWorld::_instance->_sharedFrameData[i * channelCount + j] = _sampler->GetSample(localTime, j + _channel);
			}
			localTime += sampleLength * _pitch;
		}

		_currentTime = localTime;
		*outputBuffer = ResonanceAudioWorld::_instance->_sharedFrameData;
	}

	void ResonanceAudioSource::Update()
	{
		if(_isPositional)
		{
			//For none positional sources this happens in the audio handling callback
			float *newBuffer;
			Update(960.0f / 48000.0f, 960, &newBuffer, 1);
			ResonanceAudioWorld::_instance->_audioAPI->SetInterleavedBuffer(_sourceID, newBuffer, 1, 960);
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

} // namespace RN
