//
//  RNFlexText.h
//  Rayne
//
//  Flex adapter for labels.
//

#ifndef __RN_FLEX_TEXT_H_
#define __RN_FLEX_TEXT_H_

#include "RNFlexMeasure.h"

namespace RN
{
	namespace UI
	{
		class Label;
	}
}

namespace RN
{
	class FlexText : public FlexMeasure
	{
	public:
		explicit FlexText(RN::UI::Label *label);

		YGSize Measure(float width, YGMeasureMode widthMode, float height, YGMeasureMode heightMode) override;

		RN::UI::Label *GetLabel() const { return _label; }

	private:
		RN::UI::Label *_label;
	};
} // namespace RN

#endif /* defined(__RN_FLEX_TEXT_H_) */
