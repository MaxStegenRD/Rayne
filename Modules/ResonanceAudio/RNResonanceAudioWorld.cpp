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

	//TODO: Allow to initialize with preferred device names and fall back to defaults
	ResonanceAudioWorld::ResonanceAudioWorld(ResonanceAudioSystem *audioSystem) :
		_audioSystem(audioSystem),
		_worldMasterVolume(1.0f)
	{
		RN_ASSERT(!_instance, "There already is a ResonanceAudioWorld!");
		RN_ASSERT(_audioSystem, "Audio system needs to be provided when creating an audio world!");

		for(int i = 0; i < 3; i++)
		{
			_audioSourcesSnapshots[i] = new Array();
		}

		_audioSystem->SetOwningWorld(this);
		_audioSystem->CreateListenerContext();
		_instance = this;
		_audioSystem->Retain();
	}

	ResonanceAudioWorld::~ResonanceAudioWorld()
	{
		_audioSystem->RemoveAllListenerContexts();
		_audioSystem->SetOwningWorld(nullptr);
		_audioSystem->Release();

		for(int i = 0; i < 3; i++)
		{
			SafeRelease(_audioSourcesSnapshots[i]);
		}

		_instance = nullptr;
	}

	void ResonanceAudioWorld::AddAudioSource(ResonanceAudioSource *source)
	{
		Lock();
		_audioSources.push_back(source);
		_audioSourcesSnapshotDirty = true;
		Unlock();
	}

	void ResonanceAudioWorld::RemoveAudioSource(ResonanceAudioSource *source)
	{
		Lock();
		auto iterator = std::find(_audioSources.begin(), _audioSources.end(), source);
		if(iterator != _audioSources.end())
		{
			_audioSources.erase(iterator);
			_audioSourcesSnapshotDirty = true;
		}
		Unlock();
	}

	void ResonanceAudioWorld::PublishAudioSourcesSnapshot(const std::vector<ResonanceAudioSource *> &sources)
	{
		const uint32 inUse = _audioSourcesSnapshotInUseIndex.load(std::memory_order_relaxed) % 3;
		const uint32 published = _audioSourcesSnapshotIndex.load(std::memory_order_relaxed) % 3;

		uint32 writeIndex = (published + 1) % 3;
		if(writeIndex == inUse) writeIndex = (writeIndex + 1) % 3;

		Array *fresh = _audioSourcesSnapshots[writeIndex];
		fresh->RemoveAllObjects();
		for(ResonanceAudioSource *source : sources)
		{
			if(!source) continue;
			fresh->AddObject(source);
		}

		_audioSourcesSnapshotIndex.store(writeIndex, std::memory_order_release);
	}

	void ResonanceAudioWorld::SetSimpleRoomEnabled(bool enabled)
	{
		GetAudioAPI()->EnableRoomEffects(enabled);
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

		GetAudioAPI()->SetReverbProperties(vraudio::ComputeReverbProperties(roomProperties));
		GetAudioAPI()->SetReflectionProperties(vraudio::ComputeReflectionProperties(roomProperties));

		Lock();
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

			GetAudioAPI()->SetSourceRoomEffectsGain(source->_sourceID, vraudio::ComputeRoomEffectsGain(audioSourcePosition, audioRoomPosition, audioRoomRotation, audioRoomDimensions));

			if(_raycastCallback)
			{
				float distance;
				SceneNode *listener = GetListener();
				if(listener)
					_raycastCallback(sourcePosition, listener->GetWorldPosition() - sourcePosition, distance);
				else
					distance = -1.0f;
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
					GetAudioAPI()->SetSoundObjectOcclusionIntensity(source->_sourceID, 10.0f); //Maybe make this dependent on the material
				}
				else
				{
					GetAudioAPI()->SetSoundObjectOcclusionIntensity(source->_sourceID, 0.0f);
				}
			}
		}
		Unlock();
	}

	void ResonanceAudioWorld::SetRaycastCallback(const std::function<void(Vector3, Vector3, float &distance)> &raycastCallback)
	{
		_raycastCallback = raycastCallback;
	}

	ResonanceAudioListenerState ResonanceAudioWorld::GetListenerState() const
	{
		ResonanceAudioListenerContext *ctx = GetListenerContext();
		return ctx ? ctx->GetListenerState() : ResonanceAudioListenerState();
	}

	void ResonanceAudioWorld::SetDopplerEffect(float factor, float speedOfSound)
	{
		if(!_audioSystem) return;
		for(ResonanceAudioListenerContext *ctx : _audioSystem->_listenerContexts)
		{
			if(ctx) ctx->SetDopplerEffect(factor, speedOfSound);
		}
	}

	void ResonanceAudioWorld::SetDopplerVelocitySmoothing(float oldVelocityWeight)
	{
		if(!_audioSystem) return;
		for(ResonanceAudioListenerContext *ctx : _audioSystem->_listenerContexts)
		{
			if(ctx) ctx->SetDopplerVelocitySmoothing(oldVelocityWeight);
		}
	}

	void ResonanceAudioWorld::Update(float delta)
	{
		SceneAttachment::Update(delta);

		// Publish audio sources snapshot at most once per frame (instead of on every add/remove).
		Lock();
		if(_audioSourcesSnapshotDirty)
		{
			PublishAudioSourcesSnapshot(_audioSources);
			_audioSourcesSnapshotDirty = false;
		}
		Unlock();
		
		ResonanceAudioListenerContext *ctx = GetListenerContext();
		if(ctx) ctx->Update(delta);
	}

	void ResonanceAudioWorld::SetInputSamplesCallback(std::function<void(uint32, uint32, uint32, const float *)> inputSamplesCallback)
	{
		ResonanceAudioListenerContext *ctx = GetListenerContext();
		if(ctx) ctx->SetInputSamplesCallback(std::move(inputSamplesCallback));
	}

	void ResonanceAudioWorld::SetListener(SceneNode *listener)
	{
		ResonanceAudioListenerContext *ctx = GetListenerContext();
		if(ctx) ctx->SetListener(listener);
	}

	SceneNode *ResonanceAudioWorld::GetListener() const
	{
		ResonanceAudioListenerContext *ctx = GetListenerContext();
		return ctx ? ctx->GetListener() : nullptr;
	}

	void ResonanceAudioWorld::SetMasterVolume(float volume)
	{
		_worldMasterVolume.store(volume, std::memory_order_relaxed);
	}

	void ResonanceAudioWorld::SetWetVolume(float volume)
	{
		if(!_audioSystem) return;
		for(ResonanceAudioListenerContext *ctx : _audioSystem->_listenerContexts)
		{
			if(ctx) ctx->SetWetVolume(volume);
		}
	}

	void ResonanceAudioWorld::SetDryVolume(float volume)
	{
		if(!_audioSystem) return;
		for(ResonanceAudioListenerContext *ctx : _audioSystem->_listenerContexts)
		{
			if(ctx) ctx->SetDryVolume(volume);
		}
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
