//
//  RNFlexView.cpp
//  Rayne
//
//  Minimal flex layout container.
//

#include "RNFlexView.h"

#include "RNFlexButton.h"
#include "RNFlexImage.h"
#include "RNFlexText.h"

#include <RNUILabel.h>
#include <RNUIButton.h>
#include <RNUIImageView.h>

namespace RN
{
	RNDefineMeta(FlexView, RN::UI::View)

	class FlexViewMeasure : public FlexMeasure
	{
	public:
		explicit FlexViewMeasure(FlexView *view, FlexView *parent) : _view(view), _parent(parent) {}

		YGSize Measure(float width, YGMeasureMode widthMode, float height, YGMeasureMode heightMode) override
		{
			if(!_view) return {0.0f, 0.0f};

			const RN::Vector2 intrinsicSize = _view->GetContentSize(-1.0f, -1.0f);

			const FlexStyle *style = nullptr;
			bool parentIsRow = false;
			bool hasMainAxisSize = false;
			if(_parent)
			{
				style = _parent->GetStyleForSubview(_view);
				parentIsRow = (_parent->GetDirection() == FlexDirection::Row);
				if(style)
				{
					if(parentIsRow)
					{
						hasMainAxisSize = (style->width >= 0.0f || style->flexBasis >= 0.0f);
					}
					else
					{
						hasMainAxisSize = (style->height >= 0.0f || style->flexBasis >= 0.0f);
					}
				}
			}

			float measuredWidth = intrinsicSize.x;
			if(widthMode == YGMeasureModeExactly)
			{
				measuredWidth = width;
			}
			else if(widthMode == YGMeasureModeAtMost)
			{
				if(parentIsRow && hasMainAxisSize) measuredWidth = width;
				else measuredWidth = std::min(measuredWidth, width);
			}

			float measuredHeight = 0.0f;
			if(heightMode == YGMeasureModeExactly)
			{
				measuredHeight = height;
			}
			else
			{
				const float heightWidthConstraint = (widthMode == YGMeasureModeUndefined) ? -1.0f : measuredWidth;
				const RN::Vector2 sizeForHeight = (widthMode == YGMeasureModeUndefined && heightMode == YGMeasureModeUndefined)
					? intrinsicSize
					: _view->GetContentSize(heightWidthConstraint, -1.0f);
				measuredHeight = sizeForHeight.y;
				if(heightMode == YGMeasureModeAtMost)
				{
					if(!parentIsRow && hasMainAxisSize) measuredHeight = height;
					else measuredHeight = std::min(measuredHeight, height);
				}
			}

			if(_parent && _parent->GetAlign() == FlexAlign::Stretch && _parent->GetDirection() == FlexDirection::Column)
			{
				if(!YGFloatIsUndefined(width) && widthMode != YGMeasureModeUndefined)
				{
					measuredWidth = width;
				}
				else
				{
					const RN::Vector4 padding = _parent->GetPadding();
					float parentWidth = -1.0f;
					if(FlexView *superFlex = dynamic_cast<FlexView *>(_parent->GetSuperview()))
					{
						if(const FlexStyle *parentStyle = superFlex->GetStyleForSubview(_parent))
						{
							if(parentStyle->width >= 0.0f) parentWidth = parentStyle->width;
							else if(parentStyle->minWidth >= 0.0f) parentWidth = parentStyle->minWidth;
						}
					}
					else
					{
						parentWidth = _parent->GetBounds().width;
					}

					const float innerWidth = parentWidth - padding.x - padding.z;
					if(innerWidth > 0.0f) measuredWidth = innerWidth;
				}
			}

			if(_parent && _parent->GetAlign() == FlexAlign::Stretch && _parent->GetDirection() == FlexDirection::Row)
			{
				if(!YGFloatIsUndefined(height) && heightMode != YGMeasureModeUndefined)
				{
					measuredHeight = height;
				}
				else
				{
					const RN::Vector4 padding = _parent->GetPadding();
					float parentHeight = -1.0f;
					if(FlexView *superFlex = dynamic_cast<FlexView *>(_parent->GetSuperview()))
					{
						if(const FlexStyle *parentStyle = superFlex->GetStyleForSubview(_parent))
						{
							if(parentStyle->height >= 0.0f) parentHeight = parentStyle->height;
							else if(parentStyle->minHeight >= 0.0f) parentHeight = parentStyle->minHeight;
						}
					}
					else
					{
						parentHeight = _parent->GetBounds().height;
					}

					const float innerHeight = parentHeight - padding.y - padding.w;
					if(innerHeight > 0.0f) measuredHeight = innerHeight;
				}
			}

			return {measuredWidth, measuredHeight};
		}

	private:
		FlexView *_view;
		FlexView *_parent;
	};

	static RN::Vector2 GetImageViewIntrinsicSize(RN::UI::ImageView *imageView)
	{
		if(!imageView) return RN::Vector2(0.0f, 0.0f);

		if(RN::Framebuffer *framebuffer = imageView->GetFramebuffer())
		{
			return framebuffer->GetSize();
		}

		if(RN::Texture *image = imageView->GetImage())
		{
			const RN::Texture::Descriptor &descriptor = image->GetDescriptor();
			return RN::Vector2(static_cast<float>(descriptor.width), static_cast<float>(descriptor.height));
		}

		return imageView->GetFrame().GetSize();
	}

	static RN::Vector2 GetIntrinsicSizeForView(RN::UI::View *view, bool &hasIntrinsic)
	{
		hasIntrinsic = false;
		if(!view) return RN::Vector2(0.0f, 0.0f);

		if(RN::UI::Button *button = dynamic_cast<RN::UI::Button *>(view))
		{
			RN::Vector2 size(0.0f, 0.0f);
			if(RN::UI::Label *label = button->GetLabel())
			{
				size = label->GetTextSize();
			}

			const RN::Vector2 imageSize = GetImageViewIntrinsicSize(button);
			size.x = std::max(size.x, imageSize.x);
			size.y = std::max(size.y, imageSize.y);

			hasIntrinsic = true;
			return size;
		}

		if(RN::UI::Label *label = dynamic_cast<RN::UI::Label *>(view))
		{
			hasIntrinsic = true;
			return label->GetTextSize();
		}

		if(RN::UI::ImageView *imageView = dynamic_cast<RN::UI::ImageView *>(view))
		{
			hasIntrinsic = true;
			return GetImageViewIntrinsicSize(imageView);
		}

		if(RN::FlexView *flexView = dynamic_cast<RN::FlexView *>(view))
		{
			hasIntrinsic = true;
			return flexView->GetContentSize();
		}

		return RN::Vector2(0.0f, 0.0f);
	}

	FlexView::FlexView(RN::Rect frame) :
		_rootNode(this),
		_needsLayout(true),
		_isApplyingLayout(false),
		_direction(FlexDirection::Row),
		_justify(FlexJustify::Start),
		_align(FlexAlign::Start),
		_padding(0.0f, 0.0f, 0.0f, 0.0f),
		_gap(0.0f)
	{
		_rootNode.SetDirection(_direction);
		_rootNode.SetJustify(_justify);
		_rootNode.SetAlign(_align);
		_rootNode.SetPadding(_padding);
		_rootNode.SetGap(_gap);
		SetFrame(frame);
	}

	FlexView::~FlexView()
	{
		for(auto &item : _items)
		{
			_rootNode.RemoveChild(item.node);
			delete item.node;
		}
	}

	FlexView::ItemHandle FlexView::Add(RN::UI::View *view)
	{
		if(dynamic_cast<FlexSpacerView *>(view))
		{
			FlexStyle style;
			style.flexGrow = 1.0f;
			style.flexShrink = 1.0f;
			style.flexBasis = 0.0f;
			if(style.minWidth < 0.0f) style.minWidth = 0.0f;
			if(style.minHeight < 0.0f) style.minHeight = 0.0f;
			if(_direction == FlexDirection::Row)
			{
				style.width = 0.0f;
			}
			else if(_direction == FlexDirection::Column)
			{
				style.height = 0.0f;
			}
			AddFlexSubview(view, style);
			return ItemHandle(this, view);
		}

		return Add(view, FlexStyle());
	}

	FlexView::ItemHandle FlexView::Add(RN::UI::View *view, const FlexStyle &style)
	{
		AddFlexSubview(view, style);
		return ItemHandle(this, view);
	}

	FlexView &FlexView::Direction(FlexDirection direction)
	{
		SetDirection(direction);
		return *this;
	}

	FlexView &FlexView::Justify(FlexJustify justify)
	{
		SetJustify(justify);
		return *this;
	}

	FlexView &FlexView::Align(FlexAlign align)
	{
		SetAlign(align);
		return *this;
	}

	FlexView &FlexView::Padding(const RN::Vector4 &padding)
	{
		SetPadding(padding);
		return *this;
	}

	FlexView &FlexView::Gap(float gap)
	{
		SetGap(gap);
		return *this;
	}

	void FlexView::SetDirection(FlexDirection direction)
	{
		if(_direction == direction) return;
		_direction = direction;
		_rootNode.SetDirection(direction);
		SetNeedsLayout();
	}

	void FlexView::SetJustify(FlexJustify justify)
	{
		if(_justify == justify) return;
		_justify = justify;
		_rootNode.SetJustify(justify);
		SetNeedsLayout();
	}

	void FlexView::SetAlign(FlexAlign align)
	{
		if(_align == align) return;
		_align = align;
		_rootNode.SetAlign(align);
		SetNeedsLayout();
	}

	void FlexView::SetPadding(const RN::Vector4 &padding)
	{
		_padding = padding;
		_rootNode.SetPadding(padding);
		SetNeedsLayout();
	}

	void FlexView::SetGap(float gap)
	{
		if(_gap == gap) return;
		_gap = gap;
		_rootNode.SetGap(gap);
		SetNeedsLayout();
	}

	void FlexView::AddFlexSubview(RN::UI::View *view, const FlexStyle &style)
	{
		if(!view) return;
		if(HasFlexSubview(view)) return;

		AddSubview(view);

		FlexNode *node = new FlexNode(view);
		node->SetStyle(style);

		if(RN::UI::Button *button = dynamic_cast<RN::UI::Button *>(view))
		{
			node->SetMeasure(new FlexButton(button), true);
		}
		else if(RN::UI::Label *label = dynamic_cast<RN::UI::Label *>(view))
		{
			node->SetMeasure(new FlexText(label), true);
		}
		else if(RN::UI::ImageView *imageView = dynamic_cast<RN::UI::ImageView *>(view))
		{
			node->SetMeasure(new FlexImage(imageView), true);
		}
		else if(RN::FlexView *flexView = dynamic_cast<RN::FlexView *>(view))
		{
			node->SetMeasure(new FlexViewMeasure(flexView, this), true);
		}

		bool hasIntrinsic = false;
		const RN::Vector2 intrinsicSize = GetIntrinsicSizeForView(view, hasIntrinsic);

		_rootNode.AddChild(node);
		_items.push_back({view, node, intrinsicSize, hasIntrinsic, false});
		SetNeedsLayout();
	}

	void FlexView::RemoveFlexSubview(RN::UI::View *view)
	{
		if(!view) return;
		if(view->GetSuperview() == this)
		{
			RemoveSubview(view);
			return;
		}

		for(auto it = _items.begin(); it != _items.end(); ++it)
		{
			if(it->view == view)
			{
				_rootNode.RemoveChild(it->node);
				delete it->node;
				_items.erase(it);
				SetNeedsLayout();
				break;
			}
		}
	}

	void FlexView::SetStyleForSubview(RN::UI::View *view, const FlexStyle &style)
	{
		Item *item = FindItem(view);
		if(!item) return;
		if(style.minHeight >= 0.0f)
		{
			item->intrinsicMinHeightApplied = false;
		}
		item->node->SetStyle(style);
		SetNeedsLayout();
	}

	void FlexView::SetMeasureForSubview(RN::UI::View *view, FlexMeasure *measure, bool takeOwnership)
	{
		Item *item = FindItem(view);
		if(!item) return;
		item->node->SetMeasure(measure, takeOwnership);
		SetNeedsLayout();
	}

	bool FlexView::HasFlexSubview(RN::UI::View *view) const
	{
		return FindItem(view) != nullptr;
	}

	const FlexStyle *FlexView::GetStyleForSubview(RN::UI::View *view) const
	{
		const Item *item = FindItem(view);
		return item ? &item->node->GetStyle() : nullptr;
	}

	void FlexView::SetNeedsLayout()
	{
		_needsLayout = true;

		// If this FlexView is measured as a leaf in a parent FlexView, mark that measured
		// node dirty so Yoga re-runs the parent measure callback with updated content size.
		if(FlexView *parentFlex = dynamic_cast<FlexView *>(GetSuperview()))
		{
			if(Item *item = parentFlex->FindItem(this))
			{
				if(item->node && item->node->GetMeasure())
				{
					item->node->MarkDirty();
				}
			}
			parentFlex->SetNeedsLayout();
		}
	}

	void FlexView::Update(float delta)
	{
		RN::UI::View::Update(delta);
	}

	void FlexView::SetFrame(const RN::Rect &frame)
	{
		const RN::Rect previousFrame = GetFrame();
		RN::UI::View::SetFrame(frame);
		const bool sizeChanged = (previousFrame.width != frame.width || previousFrame.height != frame.height);
		if(!sizeChanged) return;

		if(_isApplyingLayout)
		{
			// Parent layout changed our size; relayout our own subtree, but don't invalidate
			// parent layout again.
			_needsLayout = true;
			return;
		}

		SetNeedsLayout();
	}

	void FlexView::NotifyIntrinsicSizeChanged()
	{
		for(auto &item : _items)
		{
			if(item.node && item.node->GetMeasure())
			{
				item.node->MarkDirty();
			}
		}
		SetNeedsLayout();
		RN::UI::View::NotifyIntrinsicSizeChanged();
	}

	void FlexView::WillRemoveSubview(RN::UI::View *subview)
	{
		RN::UI::View::WillRemoveSubview(subview);
		if(!subview) return;

		for(auto it = _items.begin(); it != _items.end(); ++it)
		{
			if(it->view == subview)
			{
				_rootNode.RemoveChild(it->node);
				delete it->node;
				_items.erase(it);
				SetNeedsLayout();
				break;
			}
		}
	}

	void FlexView::Draw(bool isParentHidden)
	{
		if(_needsLayout)
		{
			Layout();
			_needsLayout = false;
		}

		RN::UI::View::Draw(isParentHidden);
	}

	FlexView::Item *FlexView::FindItem(RN::UI::View *view)
	{
		for(auto &item : _items)
		{
			if(item.view == view) return &item;
		}
		return nullptr;
	}

	const FlexView::Item *FlexView::FindItem(RN::UI::View *view) const
	{
		for(const auto &item : _items)
		{
			if(item.view == view) return &item;
		}
		return nullptr;
	}

	void FlexView::Layout()
	{
		if(_direction == FlexDirection::Column)
		{
			for(auto &item : _items)
			{
				const FlexStyle &style = item.node->GetStyle();
				const bool shouldUseIntrinsic = (style.height < 0.0f && style.minHeight < 0.0f);

				if(dynamic_cast<RN::FlexView *>(item.view))
				{
					if(item.intrinsicMinHeightApplied)
					{
						FlexStyle adjusted = style;
						adjusted.minHeight = -1.0f;
						item.node->SetStyle(adjusted);
						item.intrinsicMinHeightApplied = false;
					}
					continue;
				}

				bool hasIntrinsic = false;
				const RN::Vector2 intrinsicSize = GetIntrinsicSizeForView(item.view, hasIntrinsic);
				item.hasIntrinsic = hasIntrinsic;
				item.lastIntrinsic = intrinsicSize;

				if(shouldUseIntrinsic && hasIntrinsic && intrinsicSize.y > 0.0f)
				{
					FlexStyle adjusted = style;
					adjusted.minHeight = intrinsicSize.y;
					item.node->SetStyle(adjusted);
					item.intrinsicMinHeightApplied = true;
				}
				else if(item.intrinsicMinHeightApplied && !shouldUseIntrinsic)
				{
					FlexStyle adjusted = style;
					adjusted.minHeight = -1.0f;
					item.node->SetStyle(adjusted);
					item.intrinsicMinHeightApplied = false;
				}
			}
		}
		else
		{
			for(auto &item : _items)
			{
				if(item.intrinsicMinHeightApplied)
				{
					FlexStyle adjusted = item.node->GetStyle();
					adjusted.minHeight = -1.0f;
					item.node->SetStyle(adjusted);
					item.intrinsicMinHeightApplied = false;
				}
			}
		}

		const RN::Rect bounds = GetBounds();
		_isApplyingLayout = true;
		FlexLayout::Layout(&_rootNode, bounds.width, bounds.height, YGDirectionLTR, false);
		_isApplyingLayout = false;
	}

	RN::Vector2 FlexView::GetContentSize(float width, float height)
	{
		const float resolvedWidth = (width < 0.0f) ? YGUndefined : width;
		const float resolvedHeight = (height < 0.0f) ? YGUndefined : height;

		YGNodeCalculateLayout(_rootNode.GetYGNode(), resolvedWidth, resolvedHeight, YGDirectionLTR);
		const float layoutWidth = YGNodeLayoutGetWidth(_rootNode.GetYGNode());
		const float layoutHeight = YGNodeLayoutGetHeight(_rootNode.GetYGNode());
		return RN::Vector2(layoutWidth, layoutHeight);
	}

	void FlexView::SizeToFitContent(float width, float maxHeight)
	{
		const RN::Rect frame = GetFrame();
		const float resolvedWidth = (width < 0.0f) ? frame.width : width;
		const RN::Vector2 contentSize = GetContentSize(resolvedWidth, -1.0f);

		float finalHeight = contentSize.y;
		if(maxHeight >= 0.0f)
		{
			finalHeight = std::min(finalHeight, maxHeight);
		}

		SetFrame(RN::Rect(frame.x, frame.y, resolvedWidth, finalHeight));
		SetNeedsLayout();
	}

	FlexView::ItemHandle &FlexView::ItemHandle::Style(const FlexStyle &style)
	{
		if(_owner && _view) _owner->SetStyleForSubview(_view, style);
		return *this;
	}

	FlexView::ItemHandle &FlexView::ItemHandle::Measure(FlexMeasure *measure, bool takeOwnership)
	{
		if(_owner && _view) _owner->SetMeasureForSubview(_view, measure, takeOwnership);
		return *this;
	}

	FlexView::ItemHandle &FlexView::ItemHandle::Width(float value)
	{
		if(!_owner || !_view) return *this;
		const FlexStyle *current = _owner->GetStyleForSubview(_view);
		if(!current) return *this;
		FlexStyle style = *current;
		style.width = value;
		return Style(style);
	}

	FlexView::ItemHandle &FlexView::ItemHandle::Height(float value)
	{
		if(!_owner || !_view) return *this;
		const FlexStyle *current = _owner->GetStyleForSubview(_view);
		if(!current) return *this;
		FlexStyle style = *current;
		style.height = value;
		return Style(style);
	}

	FlexView::ItemHandle &FlexView::ItemHandle::Size(float width, float height)
	{
		if(!_owner || !_view) return *this;
		const FlexStyle *current = _owner->GetStyleForSubview(_view);
		if(!current) return *this;
		FlexStyle style = *current;
		style.width = width;
		style.height = height;
		return Style(style);
	}

	FlexView::ItemHandle &FlexView::ItemHandle::MinSize(float width, float height)
	{
		if(!_owner || !_view) return *this;
		const FlexStyle *current = _owner->GetStyleForSubview(_view);
		if(!current) return *this;
		FlexStyle style = *current;
		style.minWidth = width;
		style.minHeight = height;
		return Style(style);
	}

	FlexView::ItemHandle &FlexView::ItemHandle::MaxSize(float width, float height)
	{
		if(!_owner || !_view) return *this;
		const FlexStyle *current = _owner->GetStyleForSubview(_view);
		if(!current) return *this;
		FlexStyle style = *current;
		style.maxWidth = width;
		style.maxHeight = height;
		return Style(style);
	}

	FlexView::ItemHandle &FlexView::ItemHandle::Grow(float value)
	{
		if(!_owner || !_view) return *this;
		const FlexStyle *current = _owner->GetStyleForSubview(_view);
		if(!current) return *this;
		FlexStyle style = *current;
		style.flexGrow = value;
		return Style(style);
	}

	FlexView::ItemHandle &FlexView::ItemHandle::Shrink(float value)
	{
		if(!_owner || !_view) return *this;
		const FlexStyle *current = _owner->GetStyleForSubview(_view);
		if(!current) return *this;
		FlexStyle style = *current;
		style.flexShrink = value;
		return Style(style);
	}

	FlexView::ItemHandle &FlexView::ItemHandle::Basis(float value)
	{
		if(!_owner || !_view) return *this;
		const FlexStyle *current = _owner->GetStyleForSubview(_view);
		if(!current) return *this;
		FlexStyle style = *current;
		style.flexBasis = value;
		return Style(style);
	}

	FlexView::ItemHandle &FlexView::ItemHandle::BasisZero()
	{
		if(!_owner || !_view) return *this;
		const FlexStyle *current = _owner->GetStyleForSubview(_view);
		if(!current) return *this;
		FlexStyle style = *current;
		style.flexBasis = 0.0f;
		style.flexShrink = 1.0f;
		if(style.minWidth < 0.0f) style.minWidth = 0.0f;
		if(style.minHeight < 0.0f) style.minHeight = 0.0f;
		if(_owner->GetDirection() == FlexDirection::Row)
		{
			style.width = 0.0f;
		}
		else if(_owner->GetDirection() == FlexDirection::Column)
		{
			style.height = 0.0f;
		}
		return Style(style);
	}

	FlexView::ItemHandle &FlexView::ItemHandle::FlexEqual()
	{
		return BasisZero();
	}

	FlexView::ItemHandle &FlexView::ItemHandle::Margin(float all)
	{
		if(!_owner || !_view) return *this;
		const FlexStyle *current = _owner->GetStyleForSubview(_view);
		if(!current) return *this;
		FlexStyle style = *current;
		style.marginLeft = all;
		style.marginTop = all;
		style.marginRight = all;
		style.marginBottom = all;
		return Style(style);
	}

	FlexView::ItemHandle &FlexView::ItemHandle::Margin(float left, float top, float right, float bottom)
	{
		if(!_owner || !_view) return *this;
		const FlexStyle *current = _owner->GetStyleForSubview(_view);
		if(!current) return *this;
		FlexStyle style = *current;
		style.marginLeft = left;
		style.marginTop = top;
		style.marginRight = right;
		style.marginBottom = bottom;
		return Style(style);
	}
} // namespace RN
