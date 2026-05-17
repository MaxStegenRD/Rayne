//
//  RNUIInit.cpp
//  Rayne-UI
//
//  Copyright 2016 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNUIFontManager.h"
#include <Rayne.h>

RNModule(RayneUI, "net.uberpixel.rayne.ui")

RN_REGISTER_DESTRUCTOR(RayneUITeardown, RN::UI::FontManager::ReleaseSharedInstance())
