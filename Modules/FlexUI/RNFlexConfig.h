//
//  RNUIConfig.h
//  Rayne
//
//  Copyright 2016 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_FLEXUICONFIG_H_
#define __RAYNE_FLEXUICONFIG_H_

#include <Rayne.h>

#if defined(RN_BUILD_FLEXUI)
	#define FLXAPI RN_EXPORT
#else
	#define FLXAPI RN_IMPORT
#endif


#endif /* __RAYNE_FLEXUICONFIG_H_ */
