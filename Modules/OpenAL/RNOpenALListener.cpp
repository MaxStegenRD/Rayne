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

	OpenALListener::OpenALListener()
	{
	}

	OpenALListener::~OpenALListener()
	{
	}

	void OpenALListener::Update(float delta)
	{
		if(!_owner || !GetParent() || delta <= 0.0f)
			return;

		Vector3 position = GetWorldPosition();
		Vector3 velocity = position - _oldPosition;
		velocity /= delta;
		_oldPosition = position;

		Vector3 orientation[2];
		orientation[0] = GetForward();
		orientation[1] = GetUp();
		
		_owner->MakeCurrent();

		alListenerfv(AL_POSITION, &position.x);
		alListenerfv(AL_VELOCITY, &velocity.x);
		alListenerfv(AL_ORIENTATION, &orientation[0].x);
	}
} // namespace RN
