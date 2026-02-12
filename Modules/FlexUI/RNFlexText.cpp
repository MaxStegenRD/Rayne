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

	YGSize FlexText::Measure(float width, YGMeasureMode widthMode, float height, YGMeasureMode heightMode)
	{
		if(!_label)
			return YGSize {0.0f, 0.0f};

		const RN::Vector2 textSize = _label->GetTextSize();

		YGSize result;
		result.width = ResolveSize(textSize.x, width, widthMode);
		result.height = ResolveSize(textSize.y, height, heightMode);
		return result;
	}
} // namespace RN
