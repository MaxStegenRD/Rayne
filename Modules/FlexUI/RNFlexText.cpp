//
//  RNFlexText.cpp
//  Rayne
//

#include "RNFlexText.h"

#include <RNUILabel.h>

namespace RN
{
	FlexText::FlexText(RN::UI::Label *label) :
		_label(label)
	{
	}

	RN::Vector2 FlexText::Measure(float width, FlexMeasureMode widthMode, float height, FlexMeasureMode heightMode)
	{
		if(!_label)
			return RN::Vector2(0.0f, 0.0f);

		const RN::Vector2 textSize = _label->GetTextSize();
		return RN::Vector2(ResolveSize(textSize.x, width, widthMode), ResolveSize(textSize.y, height, heightMode));
	}
} // namespace RN
