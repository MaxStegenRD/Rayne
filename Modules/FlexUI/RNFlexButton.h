//
//  RNFlexButton.h
//  Rayne
//
//  Flex adapter for buttons.
//

#ifndef __RN_FLEX_BUTTON_H_
#define __RN_FLEX_BUTTON_H_

#include "RNFlexMeasure.h"

namespace RN
{
	namespace UI
	{
		class Button;
	}
}

namespace RN
{
	class FlexButton : public FlexMeasure
	{
	public:
		explicit FlexButton(RN::UI::Button *button);

		YGSize Measure(float width, YGMeasureMode widthMode, float height, YGMeasureMode heightMode) override;

		RN::UI::Button *GetButton() const { return _button; }

	private:
		RN::UI::Button *_button;
	};
} // namespace RN

#endif /* defined(__RN_FLEX_BUTTON_H_) */
