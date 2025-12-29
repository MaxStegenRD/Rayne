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

		enum class PendingAction
		{
			None,
			Seek,
			Asset,
			Stop,
			Pause,
			Play
		};

		enum class DistanceRolloffModel
		{
			Logarithmic,
			Linear,
			None
		};

		enum ControlBits : uint32_t
		{
			kWantFadeOut     = 1u << 0,
			kWantFadeIn      = 1u << 1,
			kWantSeek        = 1u << 2,
			kWantAssetChange = 1u << 3
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
		bool IsRepeating() const { return _isRepeating.load(std::memory_order_acquire); }
		bool HasTimeOfFlight() const { return _hasTimeOfFlight; }
		bool HasReverb() const { return _hasReverb; }
		RAAPI bool HasEnded() const;

		RAAPI float GetVolume() const { return _volume; }

		RN::Vector2 GetRange() const { return _minMaxRange; }
		ResonanceAudioSampler *GetSampler() const { return _sampler; }

	private:
		void SubmitPendingAction(PendingAction action);
		bool ProcessPendingActions();

		uint8 _channel;
		ResonanceAudioSampler *_sampler;

		int _sourceID;

		bool _wantsIndirectSound;
		bool _isPositional;

		bool _isPlaying;
		std::atomic<bool> _isRepeating;
		bool _isSelfdestructing;
		bool _hasTimeOfFlight;
		bool _hasReverb;

		float _volume;
		float _pitch;

		RN::Vector2 _minMaxRange;
		DistanceRolloffModel _rolloffModel;

		double _currentTime;

		std::atomic<uint32_t> _controlBits;
		std::atomic<double> _pendingSeekTime;
		std::atomic<AudioAsset*> _pendingAsset;
		std::atomic<PendingAction> _finalAction;

		int32 _fadeSamples; // >0 fade-in, <0 fade-out, 0 none

		// Cached on audio thread to use on other threads
		std::atomic<bool> _cachedHasAsset;
		std::atomic<double> _cachedTotalTime;

		RNDeclareMetaAPI(ResonanceAudioSource, RAAPI)
	};
} // namespace RN

#endif /* defined(__RAYNE_ResonanceAudioSOURCE_H_) */
