//
//  RNFlexLayout.h
//  Rayne
//
//  Yoga layout runner.
//

#ifndef __RN_FLEX_LAYOUT_H_
#define __RN_FLEX_LAYOUT_H_

#include "RNFlexNode.h"

namespace RN
{
	class FlexLayout
	{
	public:
		static void Layout(FlexNode *root, float width, float height, YGDirection direction = YGDirectionLTR, bool applyRoot = false);

	private:
		static void ApplyLayoutRecursive(FlexNode *node, const RN::Vector2 &offset, bool applySelf);
	};
} // namespace RN

#endif /* defined(__RN_FLEX_LAYOUT_H_) */
