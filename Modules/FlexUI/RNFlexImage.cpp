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

	YGSize FlexImage::Measure(float width, YGMeasureMode widthMode, float height, YGMeasureMode heightMode)
	{
		if(!_imageView)
			return YGSize {0.0f, 0.0f};

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

		YGSize result;
		result.width = ResolveSize(size.x, width, widthMode);
		result.height = ResolveSize(size.y, height, heightMode);
		return result;
	}
} // namespace RN
