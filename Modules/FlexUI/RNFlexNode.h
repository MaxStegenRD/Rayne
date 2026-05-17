//
//  RNFlexNode.h
//  Rayne
//
//  Flex layout tree node.
//

#ifndef __RN_FLEX_NODE_H_
#define __RN_FLEX_NODE_H_

#include "RNFlexMeasure.h"
#include "RNFlexStyle.h"

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
		FLXAPI explicit FlexNode(RN::UI::View *view = nullptr);
		FLXAPI  ~FlexNode();

		FLXAPI void SetView(RN::UI::View *view);
		RN::UI::View *GetView() const { return _view; }

		FLXAPI void SetStyle(const FlexStyle &style);
		const FlexStyle &GetStyle() const { return _style; }

		void SetMeasure(FlexMeasure *measure, bool takeOwnership = false);
		FlexMeasure *GetMeasure() const { return _measure; }

		FLXAPI void AddChild(FlexNode *child, size_t index = static_cast<size_t>(-1));
		FLXAPI void RemoveChild(FlexNode *child);

		FLXAPI void SetDirection(FlexDirection direction);
		FLXAPI void SetJustify(FlexJustify justify);
		FLXAPI void SetAlign(FlexAlign align);
		FLXAPI void SetPadding(const RN::Vector4 &padding);
		FLXAPI void SetGap(float gap);
		FLXAPI void MarkDirty();

	private:
		friend class FlexLayout;

		void ApplyStyle();

		RN::UI::View *_view;
		void *_node;
		FlexStyle _style;
		FlexMeasure *_measure;
		bool _ownsMeasure;
	};
} // namespace RN

#endif /* defined(__RN_FLEX_NODE_H_) */
