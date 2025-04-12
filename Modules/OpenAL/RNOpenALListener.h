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

	private:
		WeakRef<OpenALOutputDevice> _owner;
		Vector3 _oldPosition;

		RNDeclareMetaAPI(OpenALListener, OALAPI)
	};
} // namespace RN

#endif /* defined(__RAYNE_OPENALLISTENER_H_) */
