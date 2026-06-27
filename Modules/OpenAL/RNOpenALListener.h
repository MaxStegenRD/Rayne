//
//  RNOpenALListener.h
//  Rayne-OpenAL
//
//  Copyright 2017 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_OPENALLISTENER_H_
#define __RAYNE_OPENALLISTENER_H_

#include "RNOpenAL.h"

namespace RN
{
	class OpenALOutputDevice;
	class OpenALListener : public SceneNodeAttachment
	{
	friend OpenALOutputDevice;
	public:
		OALAPI OpenALListener();
		OALAPI ~OpenALListener() override;

		OALAPI void Update(float delta) override;
		OALAPI void UpdateManual(float delta);
		
		void SetManualUpdate(bool manualUpdate) { _manualUpdate = manualUpdate; }
		
		Vector3 GetLastPosition() const { return _oldPosition; }
		Quaternion GetLastRotation() const { return _rotation; }
		Vector3 GetLastVelocity() const { return _velocity; }

	private:
		WeakRef<OpenALOutputDevice> _owner;
		Vector3 _oldPosition;
		DVector3 _oldUniversePosition;
		Quaternion _rotation;
		Vector3 _velocity;
		
		bool _manualUpdate;
		bool _hasOldUniversePosition;

		RNDeclareMetaAPI(OpenALListener, OALAPI)
	};
} // namespace RN

#endif /* defined(__RAYNE_OPENALLISTENER_H_) */
