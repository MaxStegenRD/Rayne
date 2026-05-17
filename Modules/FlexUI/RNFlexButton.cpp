//
//  RNFlexButton.cpp
//  Rayne
//

#include "RNFlexButton.h"

#include <RNUIButton.h>

namespace RN
{
	FlexButton::FlexButton(RN::UI::Button *button) :
		_button(button)
	{
	}

	RN::Vector2 FlexButton::Measure(float width, FlexMeasureMode widthMode, float height, FlexMeasureMode heightMode)
	{
		if(!_button)
			return RN::Vector2(0.0f, 0.0f);

		RN::Vector2 size(0.0f, 0.0f);

		if(RN::UI::Label *label = _button->GetLabel())
		{
			size = label->GetTextSize();
		}

		if(RN::Framebuffer *framebuffer = _button->GetFramebuffer())
		{
			RN::Vector2 imageSize = framebuffer->GetSize();
			size.x = std::max(size.x, imageSize.x);
			size.y = std::max(size.y, imageSize.y);
		}
		else if(RN::Texture *image = _button->GetImage())
		{
			const RN::Texture::Descriptor &descriptor = image->GetDescriptor();
			RN::Vector2 imageSize(static_cast<float>(descriptor.width), static_cast<float>(descriptor.height));
			size.x = std::max(size.x, imageSize.x);
			size.y = std::max(size.y, imageSize.y);
		}
		else
		{
			RN::Vector2 frameSize = _button->GetFrame().GetSize();
			size.x = std::max(size.x, frameSize.x);
			size.y = std::max(size.y, frameSize.y);
		}

		return RN::Vector2(ResolveSize(size.x, width, widthMode), ResolveSize(size.y, height, heightMode));
	}
} // namespace RN
