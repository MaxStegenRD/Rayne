//
//  RNOpenALListener.cpp
//  Rayne-OpenAL
//
//  Copyright 2017 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNOpenALListener.h"
#include "RNOpenALWorld.h"

#include "AL/al.h"
#include "AL/alc.h"

namespace RN
{
	RNDefineMeta(OpenALListener, SceneNodeAttachment)

	OpenALListener::OpenALListener() :
		_manualUpdate(false),
		_hasOldUniversePosition(false)
	{
	}

	OpenALListener::~OpenALListener()
	{
	}

	void OpenALListener::Update(float delta)
	{
		if(_manualUpdate) return;
		UpdateManual(delta);
	}

	void OpenALListener::UpdateManual(float delta)
	{
		if(!_owner || !GetParent() || delta <= 0.0f)
			return;

		Vector3 position = GetWorldPosition();
		DVector3 universePosition = GetUniversePosition();
		const bool hasUniversePosition = universePosition.IsValid();

		if(hasUniversePosition && _hasOldUniversePosition)
		{
			_velocity = ((universePosition - _oldUniversePosition) / static_cast<double>(delta)).ToVector3();
			if(!_velocity.IsValid()) _velocity = Vector3();
		}
		else
		{
			_velocity = Vector3();
		}

		if(hasUniversePosition) _oldUniversePosition = universePosition;
		_hasOldUniversePosition = hasUniversePosition;
		_oldPosition = position;
		_rotation = GetWorldRotation();

		Vector3 orientation[2];
		orientation[0] = GetForward();
		orientation[1] = GetUp();
		
		_owner->MakeCurrent();

		alListenerfv(AL_POSITION, &position.x);
		alListenerfv(AL_VELOCITY, &_velocity.x);
		alListenerfv(AL_ORIENTATION, &orientation[0].x);
	}
} // namespace RN
