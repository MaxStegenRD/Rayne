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

#define CHUNK_FRAMES 3840

namespace RN
{
	RNDefineMeta(OpenALSource, SceneNode)

	OpenALSource::OpenALSource(AudioAsset *asset, size_t ignoreDeviceAtIndex) :
		_asset(nullptr),
		_isPlaying(false),
		_isRepeating(false),
		_isSelfdestructing(false),
		_hasEnded(false),
		_ringBufferTemp(nullptr),
		_isBuffering(true)
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
			
			SourceState state;
			state.sourceID = source;
			_source[device] = state;
		});

		SetAudioAsset(asset);
	}

	OpenALSource::~OpenALSource()
	{
		for(auto &pair : _source)
		{
			pair.first->MakeCurrent();
			alDeleteSources(1, &pair.second.sourceID);
			
			if(_asset && (_asset->GetType() == AudioAsset::Type::Ringbuffer || _asset->GetType() == AudioAsset::Type::Decoder))
			{
				alDeleteBuffers(pair.second.allBuffers.size(), pair.second.allBuffers.data());
			}
		}
		SafeRelease(_asset);

		if(_ringBufferTemp) delete[] _ringBufferTemp;
	}

	void OpenALSource::SetAudioAsset(AudioAsset *asset)
	{
		LockGuard<Lockable> lock(_lock);

		if(_asset == asset) return;

		bool wasPlaying = _isPlaying;
		_isPlaying = false;
		
		if(_ringBufferTemp) delete[] _ringBufferTemp;
		
		for(auto &pair : _source)
		{
			pair.first->MakeCurrent();
			pair.second.freeBuffers.clear();
			pair.second.allBuffers.clear();
			pair.second.readOffset = 0;
			alSourceStop(pair.second.sourceID);
			alSourcei(pair.second.sourceID, AL_BUFFER, 0);
			
			if(_asset && (_asset->GetType() == AudioAsset::Type::Ringbuffer || _asset->GetType() == AudioAsset::Type::Decoder))
			{
				alDeleteBuffers(pair.second.allBuffers.size(), pair.second.allBuffers.data());
			}
		}

		SafeRelease(_asset);
		_asset = asset;
		SafeRetain(_asset);

		if(!_asset) return;

		if(_asset->GetType() == AudioAsset::Type::Static)
		{
			for(auto &pair : _source)
			{
				pair.first->MakeCurrent();
				OpenALResourceAttachment *attachment = OpenALResourceAttachment::GetAttachmentForResource(asset);
				alSourcei(pair.second.sourceID, AL_BUFFER, attachment->GetBufferID(pair.first));
				alSourcei(pair.second.sourceID, AL_LOOPING, _isRepeating ? AL_TRUE : AL_FALSE);
			}
		}
		else if(_asset->GetType() == AudioAsset::Type::Ringbuffer || _asset->GetType() == AudioAsset::Type::Decoder)
		{
			_ringBufferTemp = new int16[CHUNK_FRAMES * _asset->GetChannels()];
			std::fill(_ringBufferTemp, _ringBufferTemp + CHUNK_FRAMES * _asset->GetChannels(), 0);
			
			for(auto &pair : _source)
			{
				pair.first->MakeCurrent();
				alSourcei(pair.second.sourceID, AL_LOOPING, AL_FALSE);
				ALuint buffers[3];
				alGenBuffers(3, buffers);
				pair.second.allBuffers.push_back(buffers[0]);
				pair.second.allBuffers.push_back(buffers[1]);
				pair.second.allBuffers.push_back(buffers[2]);
				
				pair.second.freeBuffers.push_back(buffers[0]);
				pair.second.freeBuffers.push_back(buffers[1]);
				pair.second.freeBuffers.push_back(buffers[2]);
			}
		}

		if(_asset && _asset->GetType() == AudioAsset::Type::Decoder)
		{
			_asset->Decode();
		}

		if(wasPlaying)
		{
			_isPlaying = true;
			
			if(_asset->GetType() != AudioAsset::Type::Ringbuffer && _asset->GetType() != AudioAsset::Type::Decoder)
			{
				for(auto &pair : _source)
				{
					pair.first->MakeCurrent();
					alSourcePlay(pair.second.sourceID);
				}
			}
		}
	}

	void OpenALSource::SetRepeat(bool repeat)
	{
		LockGuard<Lockable> lock(_lock);

		_isRepeating = repeat;

		//It will just keep playing the same buffer if looping streamed stuff
		if(!_asset || _asset->GetType() == AudioAsset::Type::Ringbuffer || _asset->GetType() == AudioAsset::Type::Decoder) return;

		for(auto &pair : _source)
		{
			pair.first->MakeCurrent();
			alSourcei(pair.second.sourceID, AL_LOOPING, repeat ? AL_TRUE : AL_FALSE);
		}
	}

	void OpenALSource::SetPitch(float pitch)
	{
		LockGuard<Lockable> lock(_lock);

		for(auto &pair : _source)
		{
			pair.first->MakeCurrent();
			alSourcef(pair.second.sourceID, AL_PITCH, pitch);
		}
	}

	void OpenALSource::SetGain(float gain)
	{
		LockGuard<Lockable> lock(_lock);

		for(auto &pair : _source)
		{
			pair.first->MakeCurrent();
			alSourcef(pair.second.sourceID, AL_GAIN, gain);
		}
	}

	void OpenALSource::SetRange(float min, float max, float rolloff)
	{
		LockGuard<Lockable> lock(_lock);

		for(auto &pair : _source)
		{
			pair.first->MakeCurrent();
			alSourcef(pair.second.sourceID, AL_REFERENCE_DISTANCE, min);
			alSourcef(pair.second.sourceID, AL_MAX_DISTANCE, max);
			alSourcef(pair.second.sourceID, AL_ROLLOFF_FACTOR, rolloff);
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

		for(auto &pair : _source)
		{
			pair.first->MakeCurrent();
			alSourcePlay(pair.second.sourceID);
		}

		_isPlaying = true;
		_hasEnded = false;
	}

	void OpenALSource::Stop()
	{
		LockGuard<Lockable> lock(_lock);

		for(auto &pair : _source)
		{
			pair.first->MakeCurrent();
			alSourceStop(pair.second.sourceID);
			alSourcePause(pair.second.sourceID);
		}

		_isPlaying = false;

		//RNDebug("Stopped: " << (_isPlaying? "true" : "false"));
	}

	void OpenALSource::Pause()
	{
		LockGuard<Lockable> lock(_lock);

		for(auto &pair : _source)
		{
			pair.first->MakeCurrent();
			alSourcePause(pair.second.sourceID);
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
			for(auto &pair : _source)
			{
				pair.first->MakeCurrent();
				alSourcef(pair.second.sourceID, AL_SEC_OFFSET, time);
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
			if(bufferedSamples > CHUNK_FRAMES * 6 && _asset->GetType() != AudioAsset::Type::Decoder)
			{
				_asset->PopData(nullptr, _asset->GetBufferedSize() - 2 * CHUNK_FRAMES * _asset->GetBytesPerSample());
				RNDebug("too much buffered audio: skipping");
			}
			else if(bufferedSamples > CHUNK_FRAMES * 2)
			{
				_isBuffering = false;
			}
			else if(_isBuffering || bufferedSamples < CHUNK_FRAMES)
			{
				_isBuffering = true;
				
				RNDebug("not enough buffered audio: waiting for more");
				
				if(!isPlaying)
				{
					for(auto &pair : _source)
					{
						alSourceStop(pair.second.sourceID);
						alSourcePause(pair.second.sourceID);
					}
					_isPlaying = false;
					hasEnded = true;
				}
			}

			size_t bytesToActuallyPop = std::numeric_limits<size_t>::max();
			for(auto &pair : _source)
			{
				ALint numberOfProcessedBuffers = 0;
				pair.first->MakeCurrent();
				alGetSourcei(pair.second.sourceID, AL_BUFFERS_PROCESSED, &numberOfProcessedBuffers);
				for(int i = 0; i < numberOfProcessedBuffers; i++)
				{
					ALuint bufferID = 0;
					alSourceUnqueueBuffers(pair.second.sourceID, 1, &bufferID);
					pair.second.freeBuffers.push_back(bufferID);
				}
				
				size_t bytesToPop = 0;
				size_t bufferedSize = pair.second.readOffset > _asset->GetBufferedSize() ? 0 : _asset->GetBufferedSize() - pair.second.readOffset;
				size_t bufferedSamples = bufferedSize / _asset->GetBytesPerSample();
				while(bufferedSamples >= CHUNK_FRAMES && !_isBuffering && pair.second.freeBuffers.size() > 0 && !hasEnded)
				{
					//TODO: Make better and don't hardcode sample type and buffer format and multiple channels
					if(_asset->GetBytesPerSample() / _asset->GetChannels() > 2)
					{
						size_t channels = _asset->GetChannels();
						std::vector<float> samplesBuffer(CHUNK_FRAMES * channels, 0.0f);
						bytesToPop = CHUNK_FRAMES * _asset->GetBytesPerSample();
						_asset->PopData(samplesBuffer.data(), bytesToPop, true, pair.second.readOffset);
						pair.second.readOffset += bytesToPop;
						
						for(size_t f = 0; f < CHUNK_FRAMES; f++)
						{
							for(size_t c = 0; c < channels; c++)
							{
								_ringBufferTemp[f * channels + c] = static_cast<int16_t>(samplesBuffer[f * channels + c] * 32767.0f);
							}
						}
					}
					else
					{
						bytesToPop = CHUNK_FRAMES * _asset->GetBytesPerSample();
						_asset->PopData(_ringBufferTemp, bytesToPop, true, pair.second.readOffset);
						pair.second.readOffset += bytesToPop;
					}
					
					//TODO: support multiple channels
					ALuint bufferID = pair.second.freeBuffers.back();
					pair.second.freeBuffers.pop_back();
					ALenum format = _asset->GetChannels() == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
					ALsizei bufferSize = CHUNK_FRAMES * sizeof(int16) * _asset->GetChannels();
					alBufferData(bufferID, format, _ringBufferTemp, bufferSize, _asset->GetSampleRate());
					alSourceQueueBuffers(pair.second.sourceID, 1, &bufferID);
					
					bufferedSize = pair.second.readOffset > _asset->GetBufferedSize() ? 0 : _asset->GetBufferedSize() - pair.second.readOffset;
					bufferedSamples = bufferedSize / _asset->GetBytesPerSample();
				}
				
				if(_isPlaying && !_isBuffering)
				{
					pair.first->MakeCurrent();
					ALenum state = AL_STOPPED;
					alGetSourcei(pair.second.sourceID, AL_SOURCE_STATE, &state);
					if(state == AL_STOPPED)
					{
						alSourcePlay(pair.second.sourceID);
						RNDebug("source stopped, restarting it");
					}
				}
				
				bytesToActuallyPop = std::min(bytesToActuallyPop, pair.second.readOffset);
			}
			
			if(bytesToActuallyPop > 0)
			{
				_asset->PopData(nullptr, bytesToActuallyPop); //Move the read head
				for(auto &pair : _source)
				{
					pair.second.readOffset -= std::min(pair.second.readOffset, bytesToActuallyPop);
					pair.second.readOffset = std::min(pair.second.readOffset, static_cast<size_t>(_asset->GetBufferedSize()));
				}
			}
		}

		ALenum sourceState = AL_STOPPED;
		_source.begin()->first->MakeCurrent();
		alGetSourcei(_source.begin()->second.sourceID, AL_SOURCE_STATE, &sourceState);
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
						
						for(auto &pair : _source)
						{
							pair.first->MakeCurrent();
							alSourcePlay(pair.second.sourceID);
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
		
		for(auto &pair : _source)
		{
			pair.first->MakeCurrent();
			alSourcefv(pair.second.sourceID, AL_POSITION, &position.x);
			alSourcefv(pair.second.sourceID, AL_VELOCITY, &_velocity.x);
		}
	}
} // namespace RN
