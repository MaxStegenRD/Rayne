//
//  RNFlexHelpers.h
//  Rayne
//
//  Helper API for building FlexUI trees.
//

#ifndef __RN_FLEX_HELPERS_H_
#define __RN_FLEX_HELPERS_H_

#include "RNFlexStyle.h"
#include "RNFlexView.h"

#include <RNUILabel.h>
#include <RNUIImageView.h>
#include <RNUIButton.h>

namespace RN
{
	class FlexStyleBuilder
	{
	public:
		FlexStyleBuilder() = default;

		FlexStyleBuilder &Width(float value)
		{
			_style.width = value;
			return *this;
		}

		FlexStyleBuilder &Height(float value)
		{
			_style.height = value;
			return *this;
		}

		FlexStyleBuilder &Size(float width, float height)
		{
			_style.width = width;
			_style.height = height;
			return *this;
		}

		FlexStyleBuilder &MinSize(float width, float height)
		{
			_style.minWidth = width;
			_style.minHeight = height;
			return *this;
		}

		FlexStyleBuilder &MaxSize(float width, float height)
		{
			_style.maxWidth = width;
			_style.maxHeight = height;
			return *this;
		}

		FlexStyleBuilder &Grow(float value)
		{
			_style.flexGrow = value;
			return *this;
		}

		FlexStyleBuilder &Shrink(float value)
		{
			_style.flexShrink = value;
			return *this;
		}

		FlexStyleBuilder &Basis(float value)
		{
			_style.flexBasis = value;
			return *this;
		}

		FlexStyleBuilder &BasisZero()
		{
			_style.flexBasis = 0.0f;
			_style.flexShrink = 1.0f;
			if(_style.minWidth < 0.0f) _style.minWidth = 0.0f;
			if(_style.minHeight < 0.0f) _style.minHeight = 0.0f;
			return *this;
		}

		FlexStyleBuilder &FlexEqual()
		{
			return BasisZero();
		}

		FlexStyleBuilder &Margin(float all)
		{
			_style.marginLeft = all;
			_style.marginTop = all;
			_style.marginRight = all;
			_style.marginBottom = all;
			return *this;
		}

		FlexStyleBuilder &Margin(float left, float top, float right, float bottom)
		{
			_style.marginLeft = left;
			_style.marginTop = top;
			_style.marginRight = right;
			_style.marginBottom = bottom;
			return *this;
		}

		operator FlexStyle() const { return _style; }

	private:
		FlexStyle _style;
	};

	namespace Flex
	{
		inline FlexStyleBuilder Style()
		{
			return FlexStyleBuilder();
		}

		inline FlexView *View(const RN::Rect &frame = Rect())
		{
			return (new FlexView(frame))->Autorelease();
		}

		inline RN::UI::Label *Label(const RN::String *text, const RN::UI::TextAttributes &attributes)
		{
			RN::UI::Label *label = (new RN::UI::Label(attributes))->Autorelease();
			if(text)
			{
				label->SetText(text);
			}
			return label;
		}

		inline RN::UI::ImageView *Image(RN::Texture *texture)
		{
			RN::UI::ImageView *image = (new RN::UI::ImageView())->Autorelease();
			if(texture)
			{
				image->SetImage(texture);
			}
			return image;
		}

		inline RN::UI::Button *Button(const RN::UI::TextAttributes &attributes, const RN::String *text = nullptr)
		{
			RN::UI::Button *button = (new RN::UI::Button(attributes))->Autorelease();
			if(text && button->GetLabel())
			{
				button->GetLabel()->SetText(text);
			}
			return button;
		}
	} // namespace Flex
} // namespace RN

#endif /* defined(__RN_FLEX_HELPERS_H_) */
