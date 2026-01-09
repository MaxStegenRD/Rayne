//
//  RNResonanceAudioWorld.cpp
//  Rayne-ResonanceAudio
//
//  Copyright 2023 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNResonanceAudioWorld.h"

#include <api/resonance_audio_api.h>
#include <platforms/common/room_effects_utils.h>

#if RN_PLATFORM_MAC_OS
	#include <AVFoundation/AVFoundation.h>
#endif

namespace RN
{
	RNDefineMeta(ResonanceAudioWorld, SceneAttachment)

	ResonanceAudioWorld *ResonanceAudioWorld::_instance = nullptr;

	ResonanceAudioWorld *ResonanceAudioWorld::GetInstance()
	{
		return _instance;
	}

	static inline float SoftClipTanhKnee(float x, float T)
	{
		const float ax = std::fabs(x);
		if(ax <= T) return x;

		const float u = (ax - T) / (1.0f - T);
		const float y = T + (1.0f - T) * std::tanhf(u);
		return std::copysign(y, x);
	}

	void ResonanceAudioWorld::AudioCallback(void *outputBuffer, const void *inputBuffer, unsigned int frameSize, unsigned int status)
	{
		AutoreleasePool pool;

		//Capture microphone samples if requested
		_instance->_audioSourcesLock.Lock();
		if(_instance->_inputSamplesCallback && inputBuffer)
		{
			const float *floatInput = static_cast<const float *>(inputBuffer);
			_instance->_inputSamplesCallback(_instance->_audioSystem->_sampleRate, _instance->_audioSystem->_channelCount, frameSize, floatInput);
		}

		if(!outputBuffer)
		{
			_instance->_audioSourcesLock.Unlock();
			return;
		}

		for(ResonanceAudioSource *source : _instance->_audioSources)
		{
			source->Update();
		}

		float masterVolume = _instance->_masterVolume.load(std::memory_order_relaxed);
		float dryVolume = _instance->_dryVolume.load(std::memory_order_relaxed);

		const uint32 channelCount = _instance->_audioSystem->_channelCount;
		const uint32 outputSampleCount = frameSize * channelCount;
		float *floatOutputBuffer = static_cast<float *>(outputBuffer);
		if(!_instance->_audioAPI->FillInterleavedOutputBuffer(channelCount, frameSize, floatOutputBuffer))
		{
			memset(floatOutputBuffer, 0, outputSampleCount * sizeof(float));
			//RNDebug("Shit. " << frameSize);
		}

		const float outputSampleRate = static_cast<float>(_instance->_audioSystem->_sampleRate);
		for(ResonanceAudioSource *source : _instance->_audioSources)
		{
			if(source->IsPositional()) continue;
			
			float *frameData = nullptr;
			if(source->Update(frameSize / outputSampleRate, frameSize, &frameData, channelCount))
			{
				for(uint32 i = 0; i < outputSampleCount; i++)
				{
					floatOutputBuffer[i] += frameData[i] * dryVolume;
				}
			}
		}
		_instance->_audioSourcesLock.Unlock();

		for(uint32 i = 0; i < outputSampleCount; i++)
		{
			floatOutputBuffer[i] = SoftClipTanhKnee(floatOutputBuffer[i] * masterVolume, 0.9f);
		}
	}

	//TODO: Allow to initialize with preferred device names and fall back to defaults
	ResonanceAudioWorld::ResonanceAudioWorld(ResonanceAudioSystem *audioSystem) :
		_audioSystem(audioSystem),
		_listener(nullptr),
		_inputBuffer(nullptr),
		_sharedFrameData(nullptr),
		_masterVolume(1.0f),
		_dryVolume(0.5f),
		_dopplerFactor(1.0f),
		_dopplerSpeedOfSound(343.3f),
		_dopplerVelocitySmoothing(0.95f),
		_dopplerListenerOldPosition(Vector3())
	{
		RN_ASSERT(!_instance, "There already is a ResonanceAudioWorld!");
		RN_ASSERT(_audioSystem, "Audio system needs to be provided when creating an audio world!");

		_audioAPI = vraudio::CreateResonanceAudioApi(_audioSystem->_channelCount, _audioSystem->_frameSize, _audioSystem->_sampleRate);
		_audioAPI->SetMasterVolume(1.0f);
		_audioAPI->EnableRoomEffects(false);

		_sharedFrameData = new float[_audioSystem->_frameSize * _audioSystem->_channelCount];
		_instance = this;
		_audioSystem->Retain();
		_audioSystem->SetAudioCallback(AudioCallback);
	}

	ResonanceAudioWorld::~ResonanceAudioWorld()
	{
		_audioSystem->Release();

		if(_sharedFrameData)
		{
			delete[] _sharedFrameData;
			_sharedFrameData = nullptr;
		}

		_instance = nullptr;
	}

	void ResonanceAudioWorld::AddAudioSource(ResonanceAudioSource *source)
	{
		_audioSourcesLock.Lock();
		_audioSources.push_back(source);
		_audioSourcesLock.Unlock();
	}

	void ResonanceAudioWorld::RemoveAudioSource(ResonanceAudioSource *source)
	{
		_audioSourcesLock.Lock();
		auto iterator = std::find(_audioSources.begin(), _audioSources.end(), source);
		if(iterator != _audioSources.end())
		{
			_audioSources.erase(iterator);
		}
		_audioSourcesLock.Unlock();
	}

	void ResonanceAudioWorld::SetSimpleRoomEnabled(bool enabled)
	{
		_audioAPI->EnableRoomEffects(enabled);
	}

	void ResonanceAudioWorld::SetSimpleRoom(Vector3 position, Vector3 dimensions, float reflectionConstant, ResonanceAudioMaterial left, ResonanceAudioMaterial right, ResonanceAudioMaterial bottom, ResonanceAudioMaterial top, ResonanceAudioMaterial front, ResonanceAudioMaterial back)
	{
		vraudio::RoomProperties roomProperties;
		roomProperties.dimensions[0] = dimensions.x;
		roomProperties.dimensions[1] = dimensions.y;
		roomProperties.dimensions[2] = dimensions.z;
		roomProperties.position[0] = position.x;
		roomProperties.position[1] = position.y;
		roomProperties.position[2] = position.z;
		roomProperties.reflection_scalar = reflectionConstant;
		roomProperties.material_names[0] = static_cast<vraudio::MaterialName>(left);
		roomProperties.material_names[1] = static_cast<vraudio::MaterialName>(right);
		roomProperties.material_names[2] = static_cast<vraudio::MaterialName>(bottom);
		roomProperties.material_names[3] = static_cast<vraudio::MaterialName>(top);
		roomProperties.material_names[4] = static_cast<vraudio::MaterialName>(front);
		roomProperties.material_names[5] = static_cast<vraudio::MaterialName>(back);

		_audioAPI->SetReverbProperties(vraudio::ComputeReverbProperties(roomProperties));
		_audioAPI->SetReflectionProperties(vraudio::ComputeReflectionProperties(roomProperties));

		_audioSourcesLock.Lock();
		for(ResonanceAudioSource *source : _instance->_audioSources)
		{
			Vector3 sourcePosition = source->GetWorldPosition();
			vraudio::WorldPosition audioSourcePosition;
			audioSourcePosition[0] = sourcePosition.x;
			audioSourcePosition[1] = sourcePosition.y;
			audioSourcePosition[2] = sourcePosition.z;

			vraudio::WorldPosition audioRoomPosition;
			audioRoomPosition[0] = position.x;
			audioRoomPosition[1] = position.y;
			audioRoomPosition[2] = position.z;

			vraudio::WorldRotation audioRoomRotation;

			vraudio::WorldPosition audioRoomDimensions;
			audioRoomDimensions[0] = dimensions.x;
			audioRoomDimensions[1] = dimensions.y;
			audioRoomDimensions[2] = dimensions.z;

			_audioAPI->SetSourceRoomEffectsGain(source->_sourceID, vraudio::ComputeRoomEffectsGain(audioSourcePosition, audioRoomPosition, audioRoomRotation, audioRoomDimensions));

			if(_raycastCallback)
			{
				float distance;
				_raycastCallback(sourcePosition, _listener->GetWorldPosition() - sourcePosition, distance);
				if(distance > -0.5f)
				{
					/*float realDistance = _listener->GetWorldPosition().GetDistance(sourcePosition);
					float otherDistance;
					_raycastCallback(_listener->GetWorldPosition(), sourcePosition - _listener->GetWorldPosition(), otherDistance);
					if(otherDistance > -0.5f)
					{
						_audioAPI->SetSoundObjectOcclusionIntensity(source->_sourceID, realDistance - distance - otherDistance);
					}
					else
					{
						_audioAPI->SetSoundObjectOcclusionIntensity(source->_sourceID, realDistance - distance);
					}*/
					_audioAPI->SetSoundObjectOcclusionIntensity(source->_sourceID, 10.0f); //Maybe make this dependent on the material
				}
				else
				{
					_audioAPI->SetSoundObjectOcclusionIntensity(source->_sourceID, 0.0f);
				}
			}
		}
		_audioSourcesLock.Unlock();
	}

	void ResonanceAudioWorld::SetRaycastCallback(const std::function<void(Vector3, Vector3, float &distance)> &raycastCallback)
	{
		_raycastCallback = raycastCallback;
	}

	ResonanceAudioWorld::ListenerState ResonanceAudioWorld::GetListenerState() const
	{
		const uint32 index = _listenerStateIndex.load(std::memory_order_acquire) & 1;
		return _listenerStateBuffers[index];
	}

	void ResonanceAudioWorld::SetDopplerEffect(float factor, float speedOfSound)
	{
		const float f = (factor > 0.0f) ? factor : 0.0f;
		const float c = (speedOfSound > 0.001f) ? speedOfSound : 0.001f;
		_dopplerFactor = f;
		_dopplerSpeedOfSound = c;
	}

	void ResonanceAudioWorld::SetDopplerVelocitySmoothing(float oldVelocityWeight)
	{
		_dopplerVelocitySmoothing = std::clamp(oldVelocityWeight, 0.0f, 0.999f);
	}

	void ResonanceAudioWorld::Update(float delta)
	{
		SceneAttachment::Update(delta);
		
		//Update listener position
		if(_listener)
		{
			Vector3 listenerPosition = _listener->GetWorldPosition();
			Quaternion listenerRotation = _listener->GetWorldRotation();
			_audioAPI->SetHeadPosition(listenerPosition.x, listenerPosition.y, listenerPosition.z);
			_audioAPI->SetHeadRotation(listenerRotation.x, listenerRotation.y, listenerRotation.z, listenerRotation.w);

			// Compute listener velocity once per frame.
			Vector3 listenerVelocity(0.0f, 0.0f, 0.0f);
			if(delta > 0.0f)
			{
				listenerVelocity = (listenerPosition - _dopplerListenerOldPosition) / delta;
				_dopplerListenerOldPosition = listenerPosition;
			}
			
			// Publish listener snapshot (lock-free, coherent multi-field read).
			const uint32 currentIndex = _listenerStateIndex.load(std::memory_order_relaxed) & 1;
			const uint32 nextIndex = currentIndex ^ 1;
			ListenerState &dst = _listenerStateBuffers[nextIndex];
			dst.position = listenerPosition;
			dst.velocity = listenerVelocity;
			dst.rotation = listenerRotation;
			dst.isValid = true;
			_listenerStateIndex.store(nextIndex, std::memory_order_release);

			//Calculate current room properties
			/*Vector3 dimensions(10.0f, 10.0f, 10.0f);
			ResonanceAudioMaterial material[6] = {ResonanceAudioMaterialBrickBare, ResonanceAudioMaterialBrickBare, ResonanceAudioMaterialBrickBare, ResonanceAudioMaterialBrickBare, ResonanceAudioMaterialBrickBare, ResonanceAudioMaterialBrickBare};

			if(_instance->_raycastCallback)
			{
				Vector3 directions[6];
				directions[0].x = -1.0f;
				directions[1].x = 1.0f;
				directions[2].y = -1.0f;
				directions[3].y = 1.0f;
				directions[4].z = -1.0f;
				directions[5].z = 1.0f;

				float distance[6];
				for(int i = 0; i < 6; i++)
				{
					_instance->_raycastCallback(listenerPosition, directions[i] * 100.0f, distance[i]);

					if(distance[i] < -0.5f)
					{
						distance[i] = 1.0f;
						material[i] = ResonanceAudioMaterialTransparent;
					}
					else
					{
						material[i] = ResonanceAudioMaterialBrickBare;
					}
				}

				dimensions.x = distance[0] + distance[1];
				dimensions.y = distance[2] + distance[3];
				dimensions.z = distance[4] + distance[5];

				//Move to actual room center
				listenerPosition.x += (distance[0] * directions[0].x + distance[1] * directions[1].x) * 0.5f;
				listenerPosition.y += (distance[2] * directions[2].x + distance[3] * directions[3].x) * 0.5f;
				listenerPosition.z += (distance[4] * directions[4].x + distance[5] * directions[5].x) * 0.5f;
			}
			SetSimpleRoom(listenerPosition, dimensions, 1.0f, material[0], material[1], material[2], material[3], material[4], material[5]);*/
		}
	}

	void ResonanceAudioWorld::SetInputBuffer(AudioAsset *inputBuffer)
	{
		RN_ASSERT(!inputBuffer || (inputBuffer->GetData()->GetLength() > 2 * _audioSystem->_frameSize), "Requires an input buffer big enough to contain two frames of audio data!");

		SafeRelease(_inputBuffer);
		_inputBuffer = inputBuffer;
		SafeRetain(_inputBuffer);
	}

	void ResonanceAudioWorld::SetInputSamplesCallback(std::function<void(uint32, uint32, uint32, const float *)> inputSamplesCallback)
	{
		_audioSourcesLock.Lock();
		_inputSamplesCallback = std::move(inputSamplesCallback);
		_audioSourcesLock.Unlock();
	}

	void ResonanceAudioWorld::SetListener(SceneNode *listener)
	{
		if(_listener)
			_listener->Release();

		_listener = nullptr;

		if(listener)
		{
			_listener = listener->Retain();
			_dopplerListenerOldPosition = _listener->GetWorldPosition();
			
			// Publish a valid snapshot immediately (velocity starts at 0).
			const uint32 currentIndex = _listenerStateIndex.load(std::memory_order_relaxed) & 1;
			const uint32 nextIndex = currentIndex ^ 1;
			ListenerState &dst = _listenerStateBuffers[nextIndex];
			dst.position = _dopplerListenerOldPosition;
			dst.velocity = Vector3(0.0f, 0.0f, 0.0f);
			dst.rotation = _listener->GetWorldRotation();
			dst.isValid = true;
			_listenerStateIndex.store(nextIndex, std::memory_order_release);
		}
		else
		{
			// Publish invalid snapshot.
			const uint32 currentIndex = _listenerStateIndex.load(std::memory_order_relaxed) & 1;
			const uint32 nextIndex = currentIndex ^ 1;
			_listenerStateBuffers[nextIndex].isValid = false;
			_listenerStateIndex.store(nextIndex, std::memory_order_release);
		}
	}

	void ResonanceAudioWorld::SetMasterVolume(float volume)
	{
		_masterVolume.store(volume, std::memory_order_relaxed);
	}

	void ResonanceAudioWorld::SetWetVolume(float volume)
	{
		_audioAPI->SetMasterVolume(volume);
	}

	void ResonanceAudioWorld::SetDryVolume(float volume)
	{
		_dryVolume.store(volume, std::memory_order_relaxed);
	}

	ResonanceAudioSource *ResonanceAudioWorld::PlaySound(AudioAsset *resource) const
	{
		ResonanceAudioSource *source = new ResonanceAudioSource(resource);
		source->Play();

		GetParent()->AddNode(source);

		return source->Autorelease();
	}

	ResonanceAudioSource *ResonanceAudioWorld::PlaySound(AudioAsset *resource, Vector3 position) const
	{
		ResonanceAudioSource *source = new ResonanceAudioSource(resource);
		source->SetWorldPosition(position);
		source->Play();

		GetParent()->AddNode(source);

		return source->Autorelease();
	}

	void ResonanceAudioWorld::RequestMicrophonePermission()
	{
		MicrophonePermissionState permissionState = GetMicrophonePermissionState();
		if(permissionState == MicrophonePermissionStateNotDetermined)
		{
#if RN_PLATFORM_MAC_OS
			if(@available(macOS 10.14, *))
			{
				[AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio
										 completionHandler:^(BOOL granted) {
										 /* if(granted)
					 {
						_inputDevice = alcCaptureOpenDevice(inputDeviceName?inputDeviceName->GetUTF8String():nullptr, 48000, AL_FORMAT_MONO16, 480);
					 }*/
										 }];
			}
#elif RN_PLATFORM_ANDROID
			android_app *app = Kernel::GetSharedInstance()->GetAndroidApp();
			JNIEnv *env = Kernel::GetSharedInstance()->GetJNIEnvForRayneMainThread();

			/*JNIEnv* env = nullptr;
			bool isNewEnv = false;

			switch(app->activity->vm->GetEnv((void**)&env, RN_JNI_VERSION_1_6))
			{
				case JNI_OK:
					break;

				case JNI_EDETACHED:
				{
					jint attachresult = app->activity->vm->AttachCurrentThread(&env, nullptr);
					if(attachresult == JNI_ERR)
					{
						RNDebug("error attaching java env to thread.");
						return;
					}

					isNewEnv = true;
					break;
				}

				case JNI_EVERSION:
					RNDebug("wrong jni version (should be 1.6)");
					return;
			}*/

			//Check for and clear any pending jni exceptions that would prevent the previous code from working
			jboolean flag = env->ExceptionCheck();
			if(flag)
			{
				env->ExceptionDescribe();
				env->ExceptionClear();
			}

			jclass activityClass = env->FindClass("android/app/NativeActivity");
			jmethodID getClassLoaderMethod = env->GetMethodID(activityClass, "getClassLoader", "()Ljava/lang/ClassLoader;");
			jobject classLoaderObject = env->CallObjectMethod(app->activity->clazz, getClassLoaderMethod);
			jclass classLoaderClass = env->FindClass("java/lang/ClassLoader");
			jmethodID loadClassMethod = env->GetMethodID(classLoaderClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");

			jstring activityCompatClassName = env->NewStringUTF("androidx.core.app.ActivityCompat");
			jclass activityCompatClass = reinterpret_cast<jclass>(env->CallObjectMethod(classLoaderObject, loadClassMethod, activityCompatClassName));
			env->DeleteLocalRef(activityCompatClassName);

			jobjectArray permissions = (jobjectArray)env->NewObjectArray(1, env->FindClass("java/lang/String"), env->NewStringUTF(""));
			env->SetObjectArrayElement(permissions, 0, env->NewStringUTF("android.permission.RECORD_AUDIO"));

			jint requestCode = 1;
			jmethodID requestPermissionsMethod = env->GetStaticMethodID(activityCompatClass, "requestPermissions", "(Landroid/app/Activity;[Ljava/lang/String;I)V");
			env->CallStaticVoidMethod(activityCompatClass, requestPermissionsMethod, app->activity->clazz, permissions, requestCode);

			env->DeleteLocalRef(permissions);

			/*if(isNewEnv)
			{
				app->activity->vm->DetachCurrentThread();
			}*/
#endif
		}
	}

	ResonanceAudioWorld::MicrophonePermissionState ResonanceAudioWorld::GetMicrophonePermissionState()
	{
#if RN_PLATFORM_MAC_OS
		// Request permission to access the microphone.
		if(@available(macOS 10.14, *))
		{
			switch([AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio])
			{
				case AVAuthorizationStatusAuthorized:
				{
					return MicrophonePermissionStateAuthorized;
				}
				case AVAuthorizationStatusNotDetermined:
				{
					return MicrophonePermissionStateNotDetermined;
				}
				case AVAuthorizationStatusDenied:
				case AVAuthorizationStatusRestricted:
					return MicrophonePermissionStateForbidden;
			}
		}
		else
		{
			// Fallback on earlier versions
			return MicrophonePermissionStateAuthorized;
		}
#elif RN_PLATFORM_ANDROID
		android_app *app = Kernel::GetSharedInstance()->GetAndroidApp();
		JNIEnv *env = Kernel::GetSharedInstance()->GetJNIEnvForRayneMainThread();

		/*JNIEnv* env = nullptr;
			bool isNewEnv = false;

			switch(app->activity->vm->GetEnv((void**)&env, RN_JNI_VERSION_1_6))
			{
				case JNI_OK:
					break;

				case JNI_EDETACHED:
				{
					jint attachresult = app->activity->vm->AttachCurrentThread(&env, nullptr);
					if(attachresult == JNI_ERR)
					{
						RNDebug("error attaching java env to threat");
						return MicrophonePermissionStateNotDetermined;
					}

					isNewEnv = true;
					break;
				}

				case JNI_EVERSION:
					RNDebug("wrong jni version (should be 1.6)");
					return MicrophonePermissionStateNotDetermined;
			}*/

		//Check for and clear any pending jni exceptions that would prevent the previous code from working
		jboolean flag = env->ExceptionCheck();
		if(flag)
		{
			env->ExceptionDescribe();
			env->ExceptionClear();
		}

		jclass activityClass = env->FindClass("android/app/NativeActivity");
		jmethodID getClassLoaderMethod = env->GetMethodID(activityClass, "getClassLoader", "()Ljava/lang/ClassLoader;");
		jobject classLoaderObject = env->CallObjectMethod(app->activity->clazz, getClassLoaderMethod);
		jclass classLoaderClass = env->FindClass("java/lang/ClassLoader");
		jmethodID loadClassMethod = env->GetMethodID(classLoaderClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");

		jstring contextCompatClassName = env->NewStringUTF("androidx.core.content.ContextCompat");
		jclass contextCompatClass = reinterpret_cast<jclass>(env->CallObjectMethod(classLoaderObject, loadClassMethod, contextCompatClassName));
		env->DeleteLocalRef(contextCompatClassName);

		jmethodID checkSelfPermissionMethod = env->GetStaticMethodID(contextCompatClass, "checkSelfPermission", "(Landroid/content/Context;Ljava/lang/String;)I");
		jstring permissionName = env->NewStringUTF("android.permission.RECORD_AUDIO");
		int returnValue = env->CallStaticIntMethod(contextCompatClass, checkSelfPermissionMethod, app->activity->clazz, permissionName);
		env->DeleteLocalRef(permissionName);

		/*if(isNewEnv)
			{
				app->activity->vm->DetachCurrentThread();
			}*/

		//Permission not granted
		if(returnValue == -1)
		{
			return MicrophonePermissionStateNotDetermined;
		}
		//Permission granted
		else if(returnValue == 0)
		{
			return MicrophonePermissionStateAuthorized;
		}
#else
		return MicrophonePermissionStateAuthorized;
#endif
		return MicrophonePermissionStateForbidden;
	}
} // namespace RN
