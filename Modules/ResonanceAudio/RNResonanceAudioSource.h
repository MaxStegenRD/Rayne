//
//  RNResonanceAudioSource.h
//  Rayne-ResonanceAudio
//
//  Copyright 2023 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_ResonanceAudioSOURCE_H_
#define __RAYNE_ResonanceAudioSOURCE_H_

#include "RNResonanceAudio.h"

namespace RN
{
	class ResonanceAudioSampler;
	struct ResonanceAudioSourceInternals;
	class ResonanceAudioSource : public SceneNode
	{
	public:
		friend class ResonanceAudioWorld;

		enum class DistanceRolloffModel
		{
			Logarithmic,
			Linear,
			None
		};

		RAAPI ResonanceAudioSource(AudioAsset *asset = nullptr, bool wantsIndirectSound = true, bool isPositional = true);
		RAAPI ~ResonanceAudioSource() override;

		RAAPI void Play();
		RAAPI void Stop();
		RAAPI void Pause();
		RAAPI void Seek(double time);

		RAAPI void SetAudioAsset(AudioAsset *asset);

		RAAPI void SetRepeat(bool repeat);
		RAAPI void SetCurrentDistanceAttenuationValue(float attentuation);
		RAAPI void SetPitch(float pitch);
		RAAPI void SetVolume(float volume);
		RAAPI void SetRange(RN::Vector2 minMaxRange);
		RAAPI void SetSelfdestruct(bool selfdestruct);
		RAAPI void SetRolloffModel(DistanceRolloffModel rolloffModel);
		RAAPI void SetChannel(uint8 channel);
		RAAPI void SetTimeOfFlight(bool tof);
		RAAPI void SetReverb(bool reverb);

		RAAPI void Update(double frameLength, uint32 sampleCount, float **outputBuffer, uint8 channelCount = 1);
		void Update();
		void DidUpdate(SceneNode::ChangeSet changeSet) override;

		bool IsPositional() const { return _isPositional; }

		bool IsPlaying() const { return _isPlaying; }
		bool IsRepeating() const { return _isRepeating; }
		bool HasTimeOfFlight() const { return _hasTimeOfFlight; }
		bool HasReverb() const { return _hasReverb; }
		RAAPI bool HasEnded() const;

		RAAPI float GetVolume() const { return _volume; }

		RN::Vector2 GetRange() const { return _minMaxRange; }
		ResonanceAudioSampler *GetSampler() const { return _sampler; }

	private:
		uint8 _channel;
		ResonanceAudioSampler *_sampler;

		int _sourceID;

		bool _wantsIndirectSound;
		bool _isPositional;

		bool _isPlaying;
		bool _isRepeating;
		bool _isSelfdestructing;
		bool _hasTimeOfFlight;
		bool _hasReverb;

		float _volume;
		float _pitch;

		RN::Vector2 _minMaxRange;
		DistanceRolloffModel _rolloffModel;

		double _currentTime;

		RNDeclareMetaAPI(ResonanceAudioSource, RAAPI)
	};
} // namespace RN

#endif /* defined(__RAYNE_ResonanceAudioSOURCE_H_) */
