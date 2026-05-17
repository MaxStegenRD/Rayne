//
//  RNFlexLayout.cpp
//  Rayne
//

#include "RNFlexLayout.h"

#include <RNUI.h>
#include <yoga/Yoga.h>

namespace RN
{
	static YGDirection ToYGDirection(FlexLayoutDirection direction)
	{
		switch(direction)
		{
			case FlexLayoutDirection::LeftToRight:
				return YGDirectionLTR;
			case FlexLayoutDirection::RightToLeft:
				return YGDirectionRTL;
		}
		return YGDirectionLTR;
	}

	void FlexLayout::Layout(FlexNode *root, float width, float height, FlexLayoutDirection direction, bool applyRoot)
	{
		if(!root) return;
		YGNodeCalculateLayout(static_cast<YGNodeRef>(root->_node), width, height, ToYGDirection(direction));
		ApplyLayoutRecursive(root, RN::Vector2(0.0f, 0.0f), applyRoot);
	}

	RN::Vector2 FlexLayout::Measure(FlexNode *root, float width, float height, FlexLayoutDirection direction)
	{
		if(!root) return RN::Vector2(0.0f, 0.0f);

		const float resolvedWidth = (width < 0.0f) ? YGUndefined : width;
		const float resolvedHeight = (height < 0.0f) ? YGUndefined : height;
		YGNodeRef rootNode = static_cast<YGNodeRef>(root->_node);
		YGNodeCalculateLayout(rootNode, resolvedWidth, resolvedHeight, ToYGDirection(direction));
		return RN::Vector2(YGNodeLayoutGetWidth(rootNode), YGNodeLayoutGetHeight(rootNode));
	}

	void FlexLayout::ApplyLayoutRecursive(FlexNode *node, const RN::Vector2 &offset, bool applySelf)
	{
		if(!node) return;

		YGNodeRef nodeRef = static_cast<YGNodeRef>(node->_node);
		const float left = YGNodeLayoutGetLeft(nodeRef);
		const float top = YGNodeLayoutGetTop(nodeRef);
		const float width = YGNodeLayoutGetWidth(nodeRef);
		const float height = YGNodeLayoutGetHeight(nodeRef);

		RN::Vector2 position(offset.x + left, offset.y + top);

		if(applySelf && node->GetView())
		{
			node->GetView()->SetFrame(RN::Rect(position.x, position.y, width, height));
		}

		const uint32 count = YGNodeGetChildCount(nodeRef);
		for(uint32 i = 0; i < count; ++i)
		{
			YGNodeRef childRef = YGNodeGetChild(nodeRef, i);
			FlexNode *childNode = static_cast<FlexNode *>(YGNodeGetContext(childRef));
			ApplyLayoutRecursive(childNode, position, true);
		}
	}
} // namespace RN
