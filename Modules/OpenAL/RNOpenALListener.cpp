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
		_hasOldWorldPosition(false)
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

		const PositionType worldPosition = GetWorldPosition();
		const bool hasWorldPosition = worldPosition.IsValid();

		if(hasWorldPosition && _hasOldWorldPosition)
		{
			_velocity = Vector3((worldPosition - _oldWorldPosition) / delta);
			if(!_velocity.IsValid()) _velocity = Vector3();
		}
		else
		{
			_velocity = Vector3();
		}

		if(hasWorldPosition) _oldWorldPosition = worldPosition;
		_hasOldWorldPosition = hasWorldPosition;
		_oldPosition = Vector3();
		_rotation = GetWorldRotation();

		Vector3 orientation[2];
		orientation[0] = GetForward();
		orientation[1] = GetUp();
		
		_owner->MakeCurrent();

		alListenerfv(AL_POSITION, &_oldPosition.x);
		alListenerfv(AL_VELOCITY, &_velocity.x);
		alListenerfv(AL_ORIENTATION, &orientation[0].x);
	}
} // namespace RN
