//
//  RNOpenALSource.cpp
//  Rayne-OpenAL
//
//  Copyright 2017 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNOpenALSource.h"
#include "RNOpenALResourceAttachment.h"
#include "RNOpenALWorld.h"

#include "AL/al.h"
#include "AL/alc.h"

namespace RN
{
	RNDefineMeta(OpenALSource, SceneNode)

	OpenALSource::OpenALSource(AudioAsset *asset, size_t ignoreDeviceAtIndex) :
		_asset(nullptr),
		_isPlaying(false),
		_isRepeating(false),
		_isSelfdestructing(false),
		_hasEnded(false),
		_ringBufferTemp(nullptr)
	{
		_oldPosition = GetWorldPosition();

		OpenALWorld::GetSharedInstance()->GetOutputDevices()->Enumerate<OpenALOutputDevice>([&](OpenALOutputDevice *device, size_t index, bool &stop){
			if(index == ignoreDeviceAtIndex) return;

			device->MakeCurrent();
			
			uint32 source;
			alGenSources(1, &source);
			alSourcef(source, AL_PITCH, 1);
			alSourcef(source, AL_GAIN, 1);
			alSourcei(source, AL_LOOPING, AL_FALSE);
			
			_source[device] = source;
		});

		SetAudioAsset(asset);
	}

	OpenALSource::~OpenALSource()
	{
		for(auto pair : _source)
		{
			pair.first->MakeCurrent();
			alDeleteSources(1, &pair.second);
			
			if(_asset && (_asset->GetType() == AudioAsset::Type::Ringbuffer || _asset->GetType() == AudioAsset::Type::Decoder))
			{
				alDeleteBuffers(1, &_ringBuffersID[0][pair.first]);
				alDeleteBuffers(1, &_ringBuffersID[1][pair.first]);
				alDeleteBuffers(1, &_ringBuffersID[2][pair.first]);
			}
		}
		SafeRelease(_asset);

		delete[] _ringBufferTemp;
	}

	void OpenALSource::SetAudioAsset(AudioAsset *asset)
	{
		LockGuard<Lockable> lock(_lock);

		if(_asset == asset) return;

		bool wasPlaying = _isPlaying;
		_isPlaying = false;
		
		for(auto pair : _source)
		{
			pair.first->MakeCurrent();
			alSourceStop(pair.second);
			alSourcei(pair.second, AL_BUFFER, 0);
			
			if(_asset && (_asset->GetType() == AudioAsset::Type::Ringbuffer || _asset->GetType() == AudioAsset::Type::Decoder))
			{
				alDeleteBuffers(1, &_ringBuffersID[0][pair.first]);
				alDeleteBuffers(1, &_ringBuffersID[1][pair.first]);
				alDeleteBuffers(1, &_ringBuffersID[2][pair.first]);
			}
		}

		SafeRelease(_asset);
		_asset = asset;
		SafeRetain(_asset);

		if(!_asset) return;

		if(_asset->GetType() == AudioAsset::Type::Static)
		{
			for(auto pair : _source)
			{
				pair.first->MakeCurrent();
				OpenALResourceAttachment *attachment = OpenALResourceAttachment::GetAttachmentForResource(asset);
				alSourcei(pair.second, AL_BUFFER, attachment->GetBufferID(pair.first));
				alSourcei(pair.second, AL_LOOPING, _isRepeating ? AL_TRUE : AL_FALSE);
			}
		}
		else if(_asset->GetType() == AudioAsset::Type::Ringbuffer || _asset->GetType() == AudioAsset::Type::Decoder)
		{
			_ringBufferTemp = new int16[3840 * _asset->GetChannels()];
			std::fill(_ringBufferTemp, _ringBufferTemp + 3840, 0);
			
			for(auto pair : _source)
			{
				pair.first->MakeCurrent();
				alSourcei(pair.second, AL_LOOPING, AL_FALSE);
				alGenBuffers(1, &_ringBuffersID[0][pair.first]);
				alGenBuffers(1, &_ringBuffersID[1][pair.first]);
				alGenBuffers(1, &_ringBuffersID[2][pair.first]);
				
				//TODO: make the format more flexible
				ALenum format = _asset->GetChannels() == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
				ALsizei bufferSize = 3840 * sizeof(int16) * _asset->GetChannels();
				alBufferData(_ringBuffersID[0][pair.first], format, _ringBufferTemp, bufferSize, _asset->GetSampleRate());
				alBufferData(_ringBuffersID[1][pair.first], format, _ringBufferTemp, bufferSize, _asset->GetSampleRate());
				alBufferData(_ringBuffersID[2][pair.first], format, _ringBufferTemp, bufferSize, _asset->GetSampleRate());
				alSourceQueueBuffers(pair.second, 1, &_ringBuffersID[0][pair.first]);
				alSourceQueueBuffers(pair.second, 1, &_ringBuffersID[1][pair.first]);
				alSourceQueueBuffers(pair.second, 1, &_ringBuffersID[2][pair.first]);
			}
		}

		if(_asset && _asset->GetType() == AudioAsset::Type::Decoder)
		{
			_asset->Decode();
		}

		if(wasPlaying)
		{
			_isPlaying = true;
			
			for(auto pair : _source)
			{
				pair.first->MakeCurrent();
				alSourcePlay(pair.second);
			}
		}
	}

	void OpenALSource::SetRepeat(bool repeat)
	{
		LockGuard<Lockable> lock(_lock);

		_isRepeating = repeat;

		//It will just keep playing the same buffer if looping streamed stuff
		if(!_asset || _asset->GetType() == AudioAsset::Type::Ringbuffer || _asset->GetType() == AudioAsset::Type::Decoder) return;

		for(auto pair : _source)
		{
			pair.first->MakeCurrent();
			alSourcei(pair.second, AL_LOOPING, repeat ? AL_TRUE : AL_FALSE);
		}
	}

	void OpenALSource::SetPitch(float pitch)
	{
		LockGuard<Lockable> lock(_lock);

		for(auto pair : _source)
		{
			pair.first->MakeCurrent();
			alSourcef(pair.second, AL_PITCH, pitch);
		}
	}

	void OpenALSource::SetGain(float gain)
	{
		LockGuard<Lockable> lock(_lock);

		for(auto pair : _source)
		{
			pair.first->MakeCurrent();
			alSourcef(pair.second, AL_GAIN, gain);
		}
	}

	void OpenALSource::SetRange(float min, float max, float rolloff)
	{
		LockGuard<Lockable> lock(_lock);

		for(auto pair : _source)
		{
			pair.first->MakeCurrent();
			alSourcef(pair.second, AL_REFERENCE_DISTANCE, min);
			alSourcef(pair.second, AL_MAX_DISTANCE, max);
			alSourcef(pair.second, AL_ROLLOFF_FACTOR, rolloff);
		}
	}

	void OpenALSource::SetSelfdestruct(bool selfdestruct)
	{
		LockGuard<Lockable> lock(_lock);

		_isSelfdestructing = selfdestruct;
	}

	void OpenALSource::Play()
	{
		LockGuard<Lockable> lock(_lock);

		UpdatePosition(0.0f);

		for(auto pair : _source)
		{
			pair.first->MakeCurrent();
			alSourcePlay(pair.second);
		}

		_isPlaying = true;
		_hasEnded = false;
	}

	void OpenALSource::Stop()
	{
		LockGuard<Lockable> lock(_lock);

		for(auto pair : _source)
		{
			pair.first->MakeCurrent();
			alSourceStop(pair.second);
			alSourcePause(pair.second);
		}

		_isPlaying = false;

		//RNDebug("Stopped: " << (_isPlaying? "true" : "false"));
	}

	void OpenALSource::Pause()
	{
		LockGuard<Lockable> lock(_lock);

		for(auto pair : _source)
		{
			pair.first->MakeCurrent();
			alSourcePause(pair.second);
		}

		_isPlaying = false;

		//RNDebug("Paused: " << (_isPlaying? "true" : "false"));
	}

	void OpenALSource::Seek(float time)
	{
		LockGuard<Lockable> lock(_lock);

		if(_asset && _asset->GetType() == AudioAsset::Type::Decoder)
		{
			_asset->Seek(time);
		}
		else
		{
			for(auto pair : _source)
			{
				pair.first->MakeCurrent();
				alSourcef(pair.second, AL_SEC_OFFSET, time);
			}
		}
	}

	bool OpenALSource::IsPlaying()
	{
		LockGuard<Lockable> lock(_lock);
		return _isPlaying;
	}

	bool OpenALSource::HasEnded()
	{
		LockGuard<Lockable> lock(_lock);
		return _hasEnded;
	}

	bool OpenALSource::IsRepeating()
	{
		LockGuard<Lockable> lock(_lock);
		return _isRepeating;
	}

	void OpenALSource::Update(float delta)
	{
		SceneNode::Update(delta);

		LockGuard<Lockable> lock(_lock);

		if(!_isPlaying) return;

		bool hasEnded = false;
		if(_asset && (_asset->GetType() == AudioAsset::Type::Ringbuffer || _asset->GetType() == AudioAsset::Type::Decoder))
		{
			bool isPlaying = _isPlaying;
			if(_asset && _asset->GetType() == AudioAsset::Type::Decoder)
			{
				isPlaying = _asset->Decode();
			}
			
			//Throw away samples if there is too much buffered audio
			uint32 bufferedSamples = _asset->GetBufferedSize() / _asset->GetBytesPerSample();
			if(bufferedSamples > 3840 * 5 && _asset->GetType() != AudioAsset::Type::Decoder)
			{
				_asset->PopData(nullptr, _asset->GetBufferedSize() - 2 * 3840 * _asset->GetBytesPerSample());
				RNDebug("too much buffered audio: skipping");
			}

			size_t samplesToActuallyPop = 100000;
			for(auto pair : _source)
			{
				ALint numberOfProcessedBuffers = 0;
				pair.first->MakeCurrent();
				alGetSourcei(pair.second, AL_BUFFERS_PROCESSED, &numberOfProcessedBuffers);
				bool needsRestart = numberOfProcessedBuffers >= 3 && _isPlaying;
				size_t samplesToPop = 0;
				while(numberOfProcessedBuffers > 0)
				{
					uint32 bufferedSize = _asset->GetBufferedSize() - _ringbufferRemainingOffset[pair.first];
					uint32 bufferedSamples = bufferedSize / _asset->GetBytesPerSample();
					if(bufferedSamples >= 3840)
					{
						//TODO: Make better and don't hardcode sample type and buffer format and multiple channels
						if(_asset->GetBytesPerSample() / _asset->GetChannels() > 2)
						{
							float samplesBuffer[3840];
							samplesToPop = 3840 * _asset->GetBytesPerSample();
							_asset->PopData(samplesBuffer, samplesToPop, true, _ringbufferRemainingOffset[pair.first]);
							_ringbufferRemainingOffset[pair.first] += samplesToPop;
							for(size_t i = 0; i < 3840; i++)
							{
								_ringBufferTemp[i] = samplesBuffer[i] * 32000.0f;
							}
						}
						else
						{
							samplesToPop = 3840 * _asset->GetBytesPerSample();
							_asset->PopData(_ringBufferTemp, samplesToPop, true, _ringbufferRemainingOffset[pair.first]);
							_ringbufferRemainingOffset[pair.first] += samplesToPop;
						}
					}
					else
					{
						if(!isPlaying)
						{
							alSourceStop(pair.second);
							alSourcePause(pair.second);
							_isPlaying = false;
							hasEnded = true;
						}
						samplesToPop = bufferedSize;
						_asset->PopData(_ringBufferTemp, samplesToPop, true, _ringbufferRemainingOffset[pair.first]);
						_ringbufferRemainingOffset[pair.first] += samplesToPop;
						std::fill(_ringBufferTemp + bufferedSamples, _ringBufferTemp + 3840, (int16)0);
						//RNDebug("not enough buffered audio: adding silence");
					}
					
					//TODO: support multiple channels
					ALuint bufferID = 0;
					ALenum format = _asset->GetChannels() == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
					ALsizei bufferSize = 3840 * sizeof(int16) * _asset->GetChannels();
					
					alSourceUnqueueBuffers(pair.second, 1, &bufferID);
					alBufferData(bufferID, format, _ringBufferTemp, bufferSize, _asset->GetSampleRate());
					alSourceQueueBuffers(pair.second, 1, &bufferID);
					
					numberOfProcessedBuffers -= 1;
				}
				
				if(needsRestart)
				{
					pair.first->MakeCurrent();
					alSourcePlay(pair.second);
				}
				
				samplesToActuallyPop = std::min(samplesToActuallyPop, _ringbufferRemainingOffset[pair.first]);
			}

			_asset->PopData(nullptr, samplesToActuallyPop); //Move the read head
			for(auto &kv : _ringbufferRemainingOffset) kv.second -= std::min(kv.second, samplesToActuallyPop);
		}

		ALenum sourceState = AL_STOPPED;
		_source.begin()->first->MakeCurrent();
		alGetSourcei(_source.begin()->second, AL_SOURCE_STATE, &sourceState);
		if((sourceState == AL_STOPPED && _asset && _asset->GetType() == AudioAsset::Type::Static) || hasEnded)
		{
			_isPlaying = false;
			_hasEnded = true;
			if(_isSelfdestructing)
			{
				if(GetSceneInfo())
					GetSceneInfo()->GetScene()->RemoveNode(this);
			}
			else
			{
				if(_asset && _asset->GetType() == AudioAsset::Type::Decoder)
				{
					_asset->Seek(0.0f);

					if(_isRepeating)
					{
						_isPlaying = true;
						_hasEnded = false;
						
						for(auto pair : _source)
						{
							pair.first->MakeCurrent();
							alSourcePlay(pair.second);
						}
					}
				}
			}
		}

		UpdatePosition(delta);
	}

	void OpenALSource::UpdatePosition(float delta)
	{
		Vector3 position = GetWorldPosition();
		
		Vector3 velocity = position - _oldPosition;
		_oldPosition = position;
		if(delta != 0.0f)
		{
			velocity /= delta;
			_velocity = _velocity * 0.95f + velocity * 0.05f; //Smoothen the velocity
		}
		
		for(auto pair : _source)
		{
			pair.first->MakeCurrent();
			alSourcefv(pair.second, AL_POSITION, &position.x);
			alSourcefv(pair.second, AL_VELOCITY, &_velocity.x);
		}
	}
} // namespace RN
