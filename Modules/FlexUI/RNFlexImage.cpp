//
//  RNFlexImage.cpp
//  Rayne
//

#include "RNFlexImage.h"

#include <RNUIImageView.h>

namespace RN
{
	FlexImage::FlexImage(RN::UI::ImageView *imageView) :
		_imageView(imageView)
	{
	}

	RN::Vector2 FlexImage::Measure(float width, FlexMeasureMode widthMode, float height, FlexMeasureMode heightMode)
	{
		if(!_imageView)
			return RN::Vector2(0.0f, 0.0f);

		RN::Vector2 size(0.0f, 0.0f);
		if(RN::Framebuffer *framebuffer = _imageView->GetFramebuffer())
		{
			size = framebuffer->GetSize();
		}
		else if(RN::Texture *image = _imageView->GetImage())
		{
			const RN::Texture::Descriptor &descriptor = image->GetDescriptor();
			size = RN::Vector2(static_cast<float>(descriptor.width), static_cast<float>(descriptor.height));
		}
		else
		{
			size = _imageView->GetFrame().GetSize();
		}

		return RN::Vector2(ResolveSize(size.x, width, widthMode), ResolveSize(size.y, height, heightMode));
	}
} // namespace RN
