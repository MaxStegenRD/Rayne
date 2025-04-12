//
//  RNOpenALWorld.h
//  Rayne-OpenAL
//
//  Copyright 2017 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_OPENALWORLD_H_
#define __RAYNE_OPENALWORLD_H_

#include "RNOpenAL.h"

#include "RNOpenALListener.h"
#include "RNOpenALSource.h"

typedef struct ALCdevice ALCdevice;
typedef struct ALCcontext ALCcontext;
namespace RN
{
	class OpenALOutputDevice : public Object
	{
	friend class OpenALWorld;
	public:
		OALAPI OpenALOutputDevice(const String *outputDeviceName = nullptr, bool loopback = false);
		OALAPI ~OpenALOutputDevice() override;
		
		OALAPI void MakeCurrent();
		
		OALAPI void SetListener(OpenALListener *attachment);
		OpenALListener *GetListener() const { return _audioListener; }
		
		OALAPI OpenALSource *PlaySound(AudioAsset *resource);
		
		OALAPI size_t GetFrameTotalSampleCount(float delta); //Only ever call this once per frame!
		void StartManualUpdate() { _isManualUpdate = true; }
		void StopManualUpdate() { _isManualUpdate = false; }
		OALAPI void GetFrameSamples(size_t sampleCount, uint8 *samples);
		
	private:
		void ProgressContext(float delta);
		
		StrongRef<OpenALListener> _audioListener;

		ALCdevice *_outputDevice;
		ALCcontext *_context;
		
		bool _isManualUpdate;
		bool _isLoopback;
		float _missingTime;

		int16 *_outputBufferTemp;

		RNDeclareMetaAPI(OpenALOutputDevice, OALAPI)
	};

	class OpenALWorld : public SceneAttachment
	{
	public:
		enum MicrophonePermissionState
		{
			MicrophonePermissionStateAuthorized,
			MicrophonePermissionStateNotDetermined,
			MicrophonePermissionStateForbidden
		};

		OALAPI OpenALWorld(const String *outputDeviceName = nullptr);
		OALAPI ~OpenALWorld() override;
		
		OALAPI void AddOutputDevice(OpenALOutputDevice *device);

		OALAPI void SetInputDevice(const String *inputDeviceName);
		OALAPI void SetInputAudioAsset(AudioAsset *bufferAsset);
		
		const Array *GetOutputDevices() const { return _outputDevices; }
		OpenALOutputDevice *GetOutputDevice(size_t index) const { return _outputDevices->GetObjectAtIndex<OpenALOutputDevice>(index); }

		OALAPI void SetDopplerEffect(float factor, float speedOfSound = 343.3);
		
		OALAPI void SetListener(OpenALListener *attachment);
		OALAPI OpenALSource *PlaySound(AudioAsset *resource);

		OALAPI static Array *GetOutputDeviceNames();
		OALAPI static Array *GetInputDeviceNames();

		OALAPI static void RequestMicrophonePermission();
		OALAPI static MicrophonePermissionState GetMicrophonePermissionState();
		
		static OpenALWorld *GetSharedInstance() { return _sharedInstance; }

	protected:
		void Update(float delta) override;

	private:
		static OpenALWorld *_sharedInstance;
		
		Array *_outputDevices;
		
		ALCdevice *_inputDevice;
		AudioAsset *_inputBuffer;
		int16 *_inputBufferTemp;

		RNDeclareMetaAPI(OpenALWorld, OALAPI)
	};
} // namespace RN

#endif /* defined(__RAYNE_OPENALWORLD_H_) */
