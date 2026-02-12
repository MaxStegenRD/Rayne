//
//  RNFlexLayout.cpp
//  Rayne
//

#include "RNFlexLayout.h"

#include <RNUI.h>

namespace RN
{
	void FlexLayout::Layout(FlexNode *root, float width, float height, YGDirection direction, bool applyRoot)
	{
		if(!root) return;
		YGNodeCalculateLayout(root->GetYGNode(), width, height, direction);
		ApplyLayoutRecursive(root, RN::Vector2(0.0f, 0.0f), applyRoot);
	}

	void FlexLayout::ApplyLayoutRecursive(FlexNode *node, const RN::Vector2 &offset, bool applySelf)
	{
		if(!node) return;

		YGNodeRef nodeRef = node->GetYGNode();
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
