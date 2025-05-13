//
//  RNNotification.cpp
//  Rayne
//
//  Copyright 2016 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNNotification.h"

namespace RN
{
	Notification::Notification(const String *name, Object *info) :
		_name(name),
		_info(info)
	{}
	Notification::~Notification()
	{
	}
} // namespace RN
