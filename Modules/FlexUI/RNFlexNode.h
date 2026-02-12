//
//  RNFlexNode.h
//  Rayne
//
//  Flex layout tree node backed by Yoga.
//

#ifndef __RN_FLEX_NODE_H_
#define __RN_FLEX_NODE_H_

#include "RNFlexMeasure.h"
#include "RNFlexStyle.h"

#include <yoga/Yoga.h>

#include <cstddef>

namespace RN
{
	namespace UI
	{
		class View;
	}
}

namespace RN
{
	class FlexNode
	{
	public:
		explicit FlexNode(RN::UI::View *view = nullptr);
		~FlexNode();

		void SetView(RN::UI::View *view);
		RN::UI::View *GetView() const { return _view; }

		YGNodeRef GetYGNode() const { return _node; }

		void SetStyle(const FlexStyle &style);
		const FlexStyle &GetStyle() const { return _style; }

		void SetMeasure(FlexMeasure *measure, bool takeOwnership = false);
		FlexMeasure *GetMeasure() const { return _measure; }

		void AddChild(FlexNode *child, size_t index = static_cast<size_t>(-1));
		void RemoveChild(FlexNode *child);

		void SetDirection(FlexDirection direction);
		void SetJustify(FlexJustify justify);
		void SetAlign(FlexAlign align);
		void SetPadding(const RN::Vector4 &padding);
		void SetGap(float gap);
		void MarkDirty();

	private:
		static YGSize MeasureCallback(const YGNode *node, float width, YGMeasureMode widthMode, float height, YGMeasureMode heightMode);
		void ApplyStyle();

		RN::UI::View *_view;
		YGNodeRef _node;
		FlexStyle _style;
		FlexMeasure *_measure;
		bool _ownsMeasure;
	};
} // namespace RN

#endif /* defined(__RN_FLEX_NODE_H_) */
