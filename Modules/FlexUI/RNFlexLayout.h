//
//  RNFlexLayout.h
//  Rayne
//
//  Flex layout runner.
//

#ifndef __RN_FLEX_LAYOUT_H_
#define __RN_FLEX_LAYOUT_H_

#include "RNFlexNode.h"

namespace RN
{
	enum class FlexLayoutDirection
	{
		LeftToRight,
		RightToLeft
	};

	class FlexLayout
	{
	public:
		static void Layout(FlexNode *root, float width, float height, FlexLayoutDirection direction = FlexLayoutDirection::LeftToRight, bool applyRoot = false);
		static RN::Vector2 Measure(FlexNode *root, float width = -1.0f, float height = -1.0f, FlexLayoutDirection direction = FlexLayoutDirection::LeftToRight);

	private:
		static void ApplyLayoutRecursive(FlexNode *node, const RN::Vector2 &offset, bool applySelf);
	};
} // namespace RN

#endif /* defined(__RN_FLEX_LAYOUT_H_) */
