//
//  RNFlexView.h
//  Rayne
//
//  Minimal flex layout container.
//

#ifndef __RN_FLEX_VIEW_H_
#define __RN_FLEX_VIEW_H_

#include "RNFlexLayout.h"
#include "RNFlexNode.h"
#include "RNFlexStyle.h"
#include <RNUI.h>
#include <Rayne.h>

#include <vector>

namespace RN
{
	class FlexView : public RN::UI::View
	{
	public:
		class ItemHandle
		{
		public:
			ItemHandle(FlexView *owner, RN::UI::View *view) : _owner(owner), _view(view) {}

			ItemHandle &Style(const FlexStyle &style);
			ItemHandle &Measure(FlexMeasure *measure, bool takeOwnership = false);
			ItemHandle &Width(float value);
			ItemHandle &Height(float value);
			ItemHandle &Size(float width, float height);
			ItemHandle &MinSize(float width, float height);
			ItemHandle &MaxSize(float width, float height);
			ItemHandle &Grow(float value);
			ItemHandle &Shrink(float value);
			ItemHandle &Basis(float value);
			ItemHandle &BasisZero();
			ItemHandle &FlexEqual();
			ItemHandle &Margin(float all);
			ItemHandle &Margin(float left, float top, float right, float bottom);

			RN::UI::View *Get() const { return _view; }

		private:
			FlexView *_owner;
			RN::UI::View *_view;
		};

		explicit FlexView(RN::Rect frame);
		~FlexView();

		ItemHandle Add(RN::UI::View *view);
		ItemHandle Add(RN::UI::View *view, const FlexStyle &style);

		FlexView &Direction(FlexDirection direction);
		FlexView &Justify(FlexJustify justify);
		FlexView &Align(FlexAlign align);
		FlexView &Padding(const RN::Vector4 &padding);
		FlexView &Gap(float gap);

		void SetDirection(FlexDirection direction);
		FlexDirection GetDirection() const { return _direction; }
		FlexAlign GetAlign() const { return _align; }
		const RN::Vector4 &GetPadding() const { return _padding; }
		void SetJustify(FlexJustify justify);
		void SetAlign(FlexAlign align);
		void SetPadding(const RN::Vector4 &padding);
		void SetGap(float gap);

		void AddFlexSubview(RN::UI::View *view, const FlexStyle &style);
		void RemoveFlexSubview(RN::UI::View *view);
		void SetStyleForSubview(RN::UI::View *view, const FlexStyle &style);
		void SetMeasureForSubview(RN::UI::View *view, FlexMeasure *measure, bool takeOwnership = false);
		bool HasFlexSubview(RN::UI::View *view) const;
		const FlexStyle *GetStyleForSubview(RN::UI::View *view) const;
		RN::Vector2 GetContentSize(float width = -1.0f, float height = -1.0f);
		void SizeToFitContent(float width, float maxHeight = -1.0f);

		void SetNeedsLayout();

	protected:
		void Update(float delta) override;
		void Draw(bool isParentHidden) override;
		void SetFrame(const RN::Rect &frame) override;
		void NotifyIntrinsicSizeChanged() override;
		void WillRemoveSubview(RN::UI::View *subview) override;

	private:
		struct Item
		{
			RN::UI::View *view;
			FlexNode *node;
			RN::Vector2 lastIntrinsic;
			bool hasIntrinsic;
			bool intrinsicMinHeightApplied = false;
		};

		void Layout();
		Item *FindItem(RN::UI::View *view);
		const Item *FindItem(RN::UI::View *view) const;

		FlexNode _rootNode;
		std::vector<Item> _items;
		bool _needsLayout;
		bool _isApplyingLayout;

		FlexDirection _direction;
		FlexJustify _justify;
		FlexAlign _align;
		RN::Vector4 _padding;
		float _gap;

		RNDeclareMeta(FlexView)
	};

	class FlexSpacerView : public FlexView
	{
	public:
		explicit FlexSpacerView(RN::Rect frame = Rect()) : FlexView(frame) {}
	};
} // namespace RN

#endif /* defined(__RN_FLEX_VIEW_H_) */
