//
//  RNOpenALSource.h
//  Rayne-OpenAL
//
//  Copyright 2017 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_OPENALSOURCE_H_
#define __RAYNE_OPENALSOURCE_H_

#include "RNOpenAL.h"

namespace RN
{
	class OpenALWorld;
	class OpenALOutputDevice;
	class OpenALSource : public SceneNode
	{
	public:
		friend OpenALWorld;

		enum AttenuationFunction
		{
			AttenuationFunctionNone,
			AttenuationFunctionLinear,
			AttenuationFunctionInverseDistanceClamped
		};

		OALAPI OpenALSource(AudioAsset *asset, size_t ignoreDeviceAtIndex = -1);
		OALAPI ~OpenALSource() override;

		OALAPI void Update(float delta) override;

		OALAPI void SetAudioAsset(AudioAsset *asset);
		OALAPI void Play();
		OALAPI void Stop();
		OALAPI void Pause();
		OALAPI void Seek(float time);
		OALAPI void SetRepeat(bool repeat);
		OALAPI void SetPitch(float pitch);
		OALAPI void SetGain(float gain);
		OALAPI void SetRange(float min, float max, float rolloff = 1.0f);
		OALAPI void SetSelfdestruct(bool selfdestruct);

		OALAPI bool IsPlaying();
		OALAPI bool HasEnded();
		OALAPI bool IsRepeating();

	private:
		void UpdatePosition(float delta);

		OpenALWorld *_owner;
		AudioAsset *_asset;

		std::map<OpenALOutputDevice*, uint32> _source;
		std::map<OpenALOutputDevice*, size_t> _ringbufferRemainingOffset;
		Vector3 _oldPosition;
		Vector3 _velocity;

		bool _isPlaying;
		bool _isRepeating;
		bool _isSelfdestructing;
		bool _hasEnded;

		std::map<OpenALOutputDevice*, uint32> _ringBuffersID[3];
		int16 *_ringBufferTemp;

		Lockable _lock;

		RNDeclareMetaAPI(OpenALSource, OALAPI)
	};
} // namespace RN

#endif /* defined(__RAYNE_OPENALSOURCE_H_) */
