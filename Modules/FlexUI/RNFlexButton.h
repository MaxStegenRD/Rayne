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
		FLXAPI explicit FlexButton(RN::UI::Button *button);

		FLXAPI RN::Vector2 Measure(float width, FlexMeasureMode widthMode, float height, FlexMeasureMode heightMode) override;

		RN::UI::Button *GetButton() const { return _button; }

	private:
		RN::UI::Button *_button;
	};
} // namespace RN

#endif /* defined(__RN_FLEX_BUTTON_H_) */
