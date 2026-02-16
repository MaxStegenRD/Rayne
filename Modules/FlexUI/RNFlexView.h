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

			FLXAPI ItemHandle &Style(const FlexStyle &style);
			FLXAPI ItemHandle &Measure(FlexMeasure *measure, bool takeOwnership = false);
			FLXAPI ItemHandle &Width(float value);
			FLXAPI ItemHandle &Height(float value);
			FLXAPI ItemHandle &Size(float width, float height);
			FLXAPI ItemHandle &MinSize(float width, float height);
			FLXAPI ItemHandle &MaxSize(float width, float height);
			FLXAPI ItemHandle &Grow(float value);
			FLXAPI ItemHandle &Shrink(float value);
			FLXAPI ItemHandle &Basis(float value);
			FLXAPI ItemHandle &BasisZero();
			FLXAPI ItemHandle &FlexEqual();
			FLXAPI ItemHandle &Margin(float all);
			FLXAPI ItemHandle &Margin(float left, float top, float right, float bottom);

			RN::UI::View *Get() const { return _view; }

		private:
			FlexView *_owner;
			RN::UI::View *_view;
		};

		FLXAPI explicit FlexView(RN::Rect frame);
		FLXAPI  ~FlexView();

		FLXAPI ItemHandle Add(RN::UI::View *view);
		FLXAPI ItemHandle Add(RN::UI::View *view, const FlexStyle &style);

		FLXAPI FlexView &Direction(FlexDirection direction);
		FLXAPI FlexView &Justify(FlexJustify justify);
		FLXAPI FlexView &Align(FlexAlign align);
		FLXAPI FlexView &Padding(const RN::Vector4 &padding);
		FLXAPI FlexView &Gap(float gap);

		FLXAPI void SetDirection(FlexDirection direction);
		FLXAPI FlexDirection GetDirection() const { return _direction; }
		FLXAPI FlexAlign GetAlign() const { return _align; }
		FLXAPI const RN::Vector4 &GetPadding() const { return _padding; }
		FLXAPI void SetJustify(FlexJustify justify);
		FLXAPI void SetAlign(FlexAlign align);
		FLXAPI void SetPadding(const RN::Vector4 &padding);
		FLXAPI void SetGap(float gap);

		FLXAPI void AddFlexSubview(RN::UI::View *view, const FlexStyle &style);
		FLXAPI void RemoveFlexSubview(RN::UI::View *view);
		FLXAPI void SetStyleForSubview(RN::UI::View *view, const FlexStyle &style);
		FLXAPI void SetMeasureForSubview(RN::UI::View *view, FlexMeasure *measure, bool takeOwnership = false);
		FLXAPI bool HasFlexSubview(RN::UI::View *view) const;
		FLXAPI const FlexStyle *GetStyleForSubview(RN::UI::View *view) const;
		FLXAPI RN::Vector2 GetContentSize(float width = -1.0f, float height = -1.0f);
		FLXAPI void SizeToFitContent(float width, float maxHeight = -1.0f);

		FLXAPI void SetNeedsLayout();

	protected:
		FLXAPI void Update(float delta) override;
		FLXAPI void Draw(bool isParentHidden) override;
		FLXAPI void SetFrame(const RN::Rect &frame) override;
		FLXAPI void NotifyIntrinsicSizeChanged() override;
		FLXAPI void WillRemoveSubview(RN::UI::View *subview) override;

	private:
		struct Item
		{
			RN::UI::View *view;
			FlexNode *node;
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

		RNDeclareMetaAPI(FlexView, FLXAPI)
	};

	class FlexSpacerView : public FlexView
	{
	public:
		explicit FlexSpacerView(RN::Rect frame = Rect()) : FlexView(frame) {}

	private:
		RNDeclareMetaAPI(FlexSpacerView, FLXAPI)
	};
} // namespace RN

#endif /* defined(__RN_FLEX_VIEW_H_) */
