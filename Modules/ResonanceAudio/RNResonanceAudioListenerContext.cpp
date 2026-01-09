//
//  RNResonanceAudioListenerContext.cpp
//  Rayne-ResonanceAudio
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNResonanceAudioListenerContext.h"
#include "RNResonanceAudioWorld.h"

#include <api/resonance_audio_api.h>

namespace RN
{
	RNDefineMeta(ResonanceAudioListenerContext, Object)

	static inline float SoftClipTanhKnee(float x, float T)
	{
		const float ax = std::fabs(x);
		if(ax <= T) return x;

		const float u = (ax - T) / (1.0f - T);
		const float y = T + (1.0f - T) * std::tanhf(u);
		return std::copysign(y, x);
	}

	ResonanceAudioListenerContext::ResonanceAudioListenerContext(ResonanceAudioWorld *world, uint32 channelCount, uint32 frameSize, uint32 sampleRate) :
		_world(world),
		_listener(nullptr),
		_oldPosition(Vector3()),
		_frameSize(frameSize),
		_channelCount(channelCount),
		_sharedFrameData(nullptr),
		_listenerMasterVolume(1.0f),
		_wetVolume(1.0f),
		_dryVolume(1.0f),
		_dopplerFactor(1.0f),
		_dopplerSpeedOfSound(343.3f),
		_dopplerVelocitySmoothing(0.95f),
		_audioAPI(nullptr)
	{
		_audioAPI = vraudio::CreateResonanceAudioApi(channelCount, frameSize, sampleRate);
		_audioAPI->SetMasterVolume(_wetVolume);
		_audioAPI->EnableRoomEffects(false);

		_sharedFrameData = new float[_frameSize * _channelCount];
	}

	ResonanceAudioListenerContext::~ResonanceAudioListenerContext()
	{
		SafeRelease(_listener);
		delete[] _sharedFrameData;
		_sharedFrameData = nullptr;
		delete _audioAPI;
		_audioAPI = nullptr;
	}

	void ResonanceAudioListenerContext::SetMasterVolume(float volume)
	{
		_listenerMasterVolume.store(volume, std::memory_order_relaxed);
	}

	void ResonanceAudioListenerContext::SetWetVolume(float volume)
	{
		const float v = (volume >= 0.0f) ? volume : 0.0f;
		_wetVolume.store(v, std::memory_order_relaxed);
		_audioAPI->SetMasterVolume(v);
	}

	void ResonanceAudioListenerContext::SetDryVolume(float volume)
	{
		_dryVolume.store(volume, std::memory_order_relaxed);
	}

	void ResonanceAudioListenerContext::SetDopplerEffect(float factor, float speedOfSound)
	{
		const float f = (factor > 0.0f) ? factor : 0.0f;
		const float c = (speedOfSound > 0.001f) ? speedOfSound : 0.001f;
		_dopplerFactor = f;
		_dopplerSpeedOfSound = c;
	}

	void ResonanceAudioListenerContext::SetDopplerVelocitySmoothing(float oldVelocityWeight)
	{
		_dopplerVelocitySmoothing = std::clamp(oldVelocityWeight, 0.0f, 0.999f);
	}

	void ResonanceAudioListenerContext::SetInputSamplesCallback(std::function<void(uint32, uint32, uint32, const float *)> inputSamplesCallback)
	{
		const uint32 currentIndex = _inputSamplesCallbackIndex.load(std::memory_order_relaxed) & 1;
		const uint32 nextIndex = currentIndex ^ 1;
		_inputSamplesCallbackBuffers[nextIndex] = std::move(inputSamplesCallback);
		_inputSamplesCallbackIndex.store(nextIndex, std::memory_order_release);
	}

	void ResonanceAudioListenerContext::SetListener(SceneNode *listener)
	{
		SafeRelease(_listener);
		_listener = SafeRetain(listener);
		_oldPosition = _listener ? _listener->GetWorldPosition() : Vector3();

		const uint32 currentIndex = _stateIndex.load(std::memory_order_relaxed) & 1;
		const uint32 nextIndex = currentIndex ^ 1;
		ResonanceAudioListenerState &dst = _stateBuffers[nextIndex];
		if(_listener)
		{
			dst.position = _oldPosition;
			dst.velocity = Vector3(0.0f, 0.0f, 0.0f);
			dst.rotation = _listener->GetWorldRotation();
			dst.isValid = true;

			_audioAPI->SetHeadPosition(dst.position.x, dst.position.y, dst.position.z);
			_audioAPI->SetHeadRotation(dst.rotation.x, dst.rotation.y, dst.rotation.z, dst.rotation.w);
		}
		else
		{
			dst.isValid = false;
		}

		_stateIndex.store(nextIndex, std::memory_order_release);
	}

	ResonanceAudioListenerState ResonanceAudioListenerContext::GetListenerState() const
	{
		const uint32 index = _stateIndex.load(std::memory_order_acquire) & 1;
		return _stateBuffers[index];
	}

	void ResonanceAudioListenerContext::Update(float delta)
	{
		if(!_listener)
			return;

		const Vector3 position = _listener->GetWorldPosition();
		const Quaternion rotation = _listener->GetWorldRotation();

		_audioAPI->SetHeadPosition(position.x, position.y, position.z);
		_audioAPI->SetHeadRotation(rotation.x, rotation.y, rotation.z, rotation.w);

		Vector3 velocity(0.0f, 0.0f, 0.0f);
		if(delta > 0.0f)
		{
			velocity = (position - _oldPosition) / delta;
			_oldPosition = position;
		}

		const uint32 currentIndex = _stateIndex.load(std::memory_order_relaxed) & 1;
		const uint32 nextIndex = currentIndex ^ 1;
		ResonanceAudioListenerState &dst = _stateBuffers[nextIndex];
		dst.position = position;
		dst.velocity = velocity;
		dst.rotation = rotation;
		dst.isValid = true;
		_stateIndex.store(nextIndex, std::memory_order_release);
	}

	void ResonanceAudioListenerContext::RenderAudio(void *outputBuffer, const void *inputBuffer, uint32 sampleRate, uint32 channelCount, uint32 frameCount, uint32 status)
	{
		AutoreleasePool pool;

		ResonanceAudioWorld *world = _world;
		if(!world)
		{
			if(outputBuffer)
			{
				memset(outputBuffer, 0, frameCount * channelCount * sizeof(float));
			}
			return;
		}

		// Capture microphone samples if requested
		const uint32 callbackIndex = _inputSamplesCallbackIndex.load(std::memory_order_acquire) & 1;
		const auto &inputSamplesCallback = _inputSamplesCallbackBuffers[callbackIndex];
		if(inputSamplesCallback && inputBuffer)
		{
			const float *floatInput = static_cast<const float *>(inputBuffer);
			inputSamplesCallback(sampleRate, channelCount, frameCount, floatInput);
		}

		if(!outputBuffer)
		{
			return;
		}

		const uint32 sourcesIndex = world->_audioSourcesSnapshotIndex.load(std::memory_order_acquire) % 3;
		world->_audioSourcesSnapshotInUseIndex.store(sourcesIndex, std::memory_order_relaxed);
		Array *sourcesSnapshot = world->_audioSourcesSnapshots[sourcesIndex];

		sourcesSnapshot->Enumerate<ResonanceAudioSource>([&](ResonanceAudioSource *source, size_t index, bool &stop){
			source->Update();
		});

		const uint32 outputSampleCount = frameCount * channelCount;
		const float worldMasterVolume = world->_worldMasterVolume.load(std::memory_order_relaxed);
		const float listenerMasterVolume = _listenerMasterVolume.load(std::memory_order_relaxed);
		const float dryVolume = _dryVolume.load(std::memory_order_relaxed);
		const float finalVolume = worldMasterVolume * listenerMasterVolume;

		float *floatOutputBuffer = static_cast<float *>(outputBuffer);
		if(!GetAudioAPI()->FillInterleavedOutputBuffer(channelCount, frameCount, floatOutputBuffer))
		{
			memset(floatOutputBuffer, 0, outputSampleCount * sizeof(float));
		}

		const float outputSampleRate = static_cast<float>(sampleRate);
		sourcesSnapshot->Enumerate<ResonanceAudioSource>([&](ResonanceAudioSource *source, size_t index, bool &stop){
			if(source->IsPositional()) return;

			float *frameData = _sharedFrameData;
			if(source->Update(frameCount / outputSampleRate, frameCount, &frameData, channelCount))
			{
				for(uint32 i = 0; i < outputSampleCount; i++)
				{
					floatOutputBuffer[i] += frameData[i] * dryVolume;
				}
			}
		});

		for(uint32 i = 0; i < outputSampleCount; i++)
		{
			floatOutputBuffer[i] = SoftClipTanhKnee(floatOutputBuffer[i] * finalVolume, 0.9f);
		}
	}
} // namespace RN

