//
//  RNFlexNode.cpp
//  Rayne
//

#include "RNFlexNode.h"

#include <RNUI.h>
#include <yoga/Yoga.h>

namespace RN
{
	static YGNodeRef GetYGNode(void *node)
	{
		return static_cast<YGNodeRef>(node);
	}

	static FlexMeasureMode ToFlexMeasureMode(YGMeasureMode mode)
	{
		switch(mode)
		{
			case YGMeasureModeUndefined:
				return FlexMeasureMode::Undefined;
			case YGMeasureModeExactly:
				return FlexMeasureMode::Exactly;
			case YGMeasureModeAtMost:
				return FlexMeasureMode::AtMost;
		}
		return FlexMeasureMode::Undefined;
	}

	static YGSize MeasureCallback(const YGNode *node, float width, YGMeasureMode widthMode, float height, YGMeasureMode heightMode)
	{
		FlexNode *flexNode = static_cast<FlexNode *>(YGNodeGetContext(node));
		if(!flexNode || !flexNode->GetMeasure())
		{
			return YGSize {0.0f, 0.0f};
		}

		const RN::Vector2 size = flexNode->GetMeasure()->Measure(width, ToFlexMeasureMode(widthMode), height, ToFlexMeasureMode(heightMode));
		return YGSize {size.x, size.y};
	}

	static YGFlexDirection ToYGFlexDirection(FlexDirection direction)
	{
		switch(direction)
		{
			case FlexDirection::Row:
				return YGFlexDirectionRow;
			case FlexDirection::Column:
				return YGFlexDirectionColumn;
		}
		return YGFlexDirectionRow;
	}

	static YGJustify ToYGJustify(FlexJustify justify)
	{
		switch(justify)
		{
			case FlexJustify::Start:
				return YGJustifyFlexStart;
			case FlexJustify::Center:
				return YGJustifyCenter;
			case FlexJustify::End:
				return YGJustifyFlexEnd;
			case FlexJustify::SpaceBetween:
				return YGJustifySpaceBetween;
			case FlexJustify::SpaceAround:
				return YGJustifySpaceAround;
		}
		return YGJustifyFlexStart;
	}

	static YGAlign ToYGAlign(FlexAlign align)
	{
		switch(align)
		{
			case FlexAlign::Start:
				return YGAlignFlexStart;
			case FlexAlign::Center:
				return YGAlignCenter;
			case FlexAlign::End:
				return YGAlignFlexEnd;
			case FlexAlign::Stretch:
				return YGAlignStretch;
		}
		return YGAlignFlexStart;
	}

	FlexNode::FlexNode(RN::UI::View *view) :
		_view(view),
		_node(YGNodeNew()),
		_measure(nullptr),
		_ownsMeasure(false)
	{
		YGNodeRef node = GetYGNode(_node);
		YGNodeSetContext(node, this);
		ApplyStyle();
	}

	FlexNode::~FlexNode()
	{
		if(_ownsMeasure)
		{
			delete _measure;
		}
		_measure = nullptr;
		YGNodeRef node = GetYGNode(_node);
		const uint32 childCount = YGNodeGetChildCount(node);
		RN_ASSERT(childCount == 0, "FlexNode destroyed while still owning child nodes. Remove children before destroying the parent.");
		while(YGNodeGetChildCount(node) > 0)
		{
			YGNodeRemoveChild(node, YGNodeGetChild(node, 0));
		}
		YGNodeFree(node);
	}

	void FlexNode::SetView(RN::UI::View *view)
	{
		_view = view;
	}

	void FlexNode::SetStyle(const FlexStyle &style)
	{
		_style = style;
		ApplyStyle();
	}

	void FlexNode::SetMeasure(FlexMeasure *measure, bool takeOwnership)
	{
		YGNodeRef node = GetYGNode(_node);
		if(measure && YGNodeGetChildCount(node) > 0)
		{
			return;
		}

		if(_ownsMeasure && _measure != measure)
		{
			delete _measure;
		}

		_measure = measure;
		_ownsMeasure = takeOwnership;

		if(_measure)
		{
			YGNodeSetMeasureFunc(node, static_cast<YGMeasureFunc>(&MeasureCallback));
		}
		else
		{
			YGNodeSetMeasureFunc(node, nullptr);
		}
	}

	void FlexNode::AddChild(FlexNode *child, size_t index)
	{
		if(!child) return;
		if(_measure) return;
		YGNodeRef node = GetYGNode(_node);
		YGNodeRef childNode = GetYGNode(child->_node);
		const uint32 count = YGNodeGetChildCount(node);
		const uint32 insertIndex = (index == static_cast<size_t>(-1) || index > count) ? count : static_cast<uint32>(index);
		YGNodeInsertChild(node, childNode, insertIndex);
	}

	void FlexNode::RemoveChild(FlexNode *child)
	{
		if(!child) return;
		YGNodeRef node = GetYGNode(_node);
		YGNodeRef childNode = GetYGNode(child->_node);
		YGNodeRemoveChild(node, childNode);
	}

	void FlexNode::SetDirection(FlexDirection direction)
	{
		YGNodeRef node = GetYGNode(_node);
		YGNodeStyleSetFlexDirection(node, ToYGFlexDirection(direction));
	}

	void FlexNode::SetJustify(FlexJustify justify)
	{
		YGNodeRef node = GetYGNode(_node);
		YGNodeStyleSetJustifyContent(node, ToYGJustify(justify));
	}

	void FlexNode::SetAlign(FlexAlign align)
	{
		YGNodeRef node = GetYGNode(_node);
		YGNodeStyleSetAlignItems(node, ToYGAlign(align));
	}

	void FlexNode::SetPadding(const RN::Vector4 &padding)
	{
		YGNodeRef node = GetYGNode(_node);
		YGNodeStyleSetPadding(node, YGEdgeLeft, padding.x);
		YGNodeStyleSetPadding(node, YGEdgeTop, padding.y);
		YGNodeStyleSetPadding(node, YGEdgeRight, padding.z);
		YGNodeStyleSetPadding(node, YGEdgeBottom, padding.w);
	}

	void FlexNode::SetGap(float gap)
	{
		YGNodeRef node = GetYGNode(_node);
		YGNodeStyleSetGap(node, YGGutterAll, gap);
	}

	void FlexNode::MarkDirty()
	{
		if(_measure)
		{
			YGNodeRef node = GetYGNode(_node);
			YGNodeMarkDirty(node);
		}
	}

	void FlexNode::ApplyStyle()
	{
		YGNodeRef node = GetYGNode(_node);
		if(_style.width >= 0.0f)
			YGNodeStyleSetWidth(node, _style.width);
		else
			YGNodeStyleSetWidthAuto(node);

		if(_style.height >= 0.0f)
			YGNodeStyleSetHeight(node, _style.height);
		else
			YGNodeStyleSetHeightAuto(node);

		YGNodeStyleSetMinWidth(node, _style.minWidth >= 0.0f ? _style.minWidth : YGUndefined);
		YGNodeStyleSetMinHeight(node, _style.minHeight >= 0.0f ? _style.minHeight : YGUndefined);
		YGNodeStyleSetMaxWidth(node, _style.maxWidth >= 0.0f ? _style.maxWidth : YGUndefined);
		YGNodeStyleSetMaxHeight(node, _style.maxHeight >= 0.0f ? _style.maxHeight : YGUndefined);

		YGNodeStyleSetFlexGrow(node, std::max(_style.flexGrow, 0.0f));
		YGNodeStyleSetFlexShrink(node, std::max(_style.flexShrink, 0.0f));
		if(_style.flexBasis >= 0.0f)
			YGNodeStyleSetFlexBasis(node, _style.flexBasis);
		else
			YGNodeStyleSetFlexBasisAuto(node);

		YGNodeStyleSetMargin(node, YGEdgeLeft, _style.marginLeft);
		YGNodeStyleSetMargin(node, YGEdgeTop, _style.marginTop);
		YGNodeStyleSetMargin(node, YGEdgeRight, _style.marginRight);
		YGNodeStyleSetMargin(node, YGEdgeBottom, _style.marginBottom);
	}
} // namespace RN
