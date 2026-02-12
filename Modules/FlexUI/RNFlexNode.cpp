//
//  RNFlexNode.cpp
//  Rayne
//

#include "RNFlexNode.h"

#include <RNUI.h>

namespace RN
{
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
		YGNodeSetContext(_node, this);
		ApplyStyle();
	}

	FlexNode::~FlexNode()
	{
		if(_ownsMeasure)
		{
			delete _measure;
		}
		_measure = nullptr;
		const uint32 childCount = YGNodeGetChildCount(_node);
		RN_ASSERT(childCount == 0, "FlexNode destroyed while still owning child nodes. Remove children before destroying the parent.");
		while(YGNodeGetChildCount(_node) > 0)
		{
			YGNodeRemoveChild(_node, YGNodeGetChild(_node, 0));
		}
		YGNodeFree(_node);
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
		if(measure && YGNodeGetChildCount(_node) > 0)
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
			YGNodeSetMeasureFunc(_node, static_cast<YGMeasureFunc>(&FlexNode::MeasureCallback));
		}
		else
		{
			YGNodeSetMeasureFunc(_node, nullptr);
		}
	}

	void FlexNode::AddChild(FlexNode *child, size_t index)
	{
		if(!child) return;
		if(_measure) return;
		const uint32 count = YGNodeGetChildCount(_node);
		const uint32 insertIndex = (index == static_cast<size_t>(-1) || index > count) ? count : static_cast<uint32>(index);
		YGNodeInsertChild(_node, child->_node, insertIndex);
	}

	void FlexNode::RemoveChild(FlexNode *child)
	{
		if(!child) return;
		YGNodeRemoveChild(_node, child->_node);
	}

	void FlexNode::SetDirection(FlexDirection direction)
	{
		YGNodeStyleSetFlexDirection(_node, ToYGFlexDirection(direction));
	}

	void FlexNode::SetJustify(FlexJustify justify)
	{
		YGNodeStyleSetJustifyContent(_node, ToYGJustify(justify));
	}

	void FlexNode::SetAlign(FlexAlign align)
	{
		YGNodeStyleSetAlignItems(_node, ToYGAlign(align));
	}

	void FlexNode::SetPadding(const RN::Vector4 &padding)
	{
		YGNodeStyleSetPadding(_node, YGEdgeLeft, padding.x);
		YGNodeStyleSetPadding(_node, YGEdgeTop, padding.y);
		YGNodeStyleSetPadding(_node, YGEdgeRight, padding.z);
		YGNodeStyleSetPadding(_node, YGEdgeBottom, padding.w);
	}

	void FlexNode::SetGap(float gap)
	{
		YGNodeStyleSetGap(_node, YGGutterAll, gap);
	}

	void FlexNode::MarkDirty()
	{
		if(_measure)
		{
			YGNodeMarkDirty(_node);
		}
	}

	YGSize FlexNode::MeasureCallback(const YGNode *node, float width, YGMeasureMode widthMode, float height, YGMeasureMode heightMode)
	{
		FlexNode *flexNode = static_cast<FlexNode *>(YGNodeGetContext(node));
		if(!flexNode || !flexNode->_measure)
		{
			return YGSize {0.0f, 0.0f};
		}

		return flexNode->_measure->Measure(width, widthMode, height, heightMode);
	}

	void FlexNode::ApplyStyle()
	{
		if(_style.width >= 0.0f)
			YGNodeStyleSetWidth(_node, _style.width);
		else
			YGNodeStyleSetWidthAuto(_node);

		if(_style.height >= 0.0f)
			YGNodeStyleSetHeight(_node, _style.height);
		else
			YGNodeStyleSetHeightAuto(_node);

		YGNodeStyleSetMinWidth(_node, _style.minWidth >= 0.0f ? _style.minWidth : YGUndefined);
		YGNodeStyleSetMinHeight(_node, _style.minHeight >= 0.0f ? _style.minHeight : YGUndefined);
		YGNodeStyleSetMaxWidth(_node, _style.maxWidth >= 0.0f ? _style.maxWidth : YGUndefined);
		YGNodeStyleSetMaxHeight(_node, _style.maxHeight >= 0.0f ? _style.maxHeight : YGUndefined);

		YGNodeStyleSetFlexGrow(_node, std::max(_style.flexGrow, 0.0f));
		YGNodeStyleSetFlexShrink(_node, std::max(_style.flexShrink, 0.0f));
		if(_style.flexBasis >= 0.0f)
			YGNodeStyleSetFlexBasis(_node, _style.flexBasis);
		else
			YGNodeStyleSetFlexBasisAuto(_node);

		YGNodeStyleSetMargin(_node, YGEdgeLeft, _style.marginLeft);
		YGNodeStyleSetMargin(_node, YGEdgeTop, _style.marginTop);
		YGNodeStyleSetMargin(_node, YGEdgeRight, _style.marginRight);
		YGNodeStyleSetMargin(_node, YGEdgeBottom, _style.marginBottom);
	}
} // namespace RN
