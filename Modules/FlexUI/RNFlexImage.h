//
//  RNFlexImage.h
//  Rayne
//
//  Flex adapter for images.
//

#ifndef __RN_FLEX_IMAGE_H_
#define __RN_FLEX_IMAGE_H_

#include "RNFlexMeasure.h"

namespace RN
{
	namespace UI
	{
		class ImageView;
	}
}

namespace RN
{
	class FlexImage : public FlexMeasure
	{
	public:
		FLXAPI explicit FlexImage(RN::UI::ImageView *imageView);

		FLXAPI YGSize Measure(float width, YGMeasureMode widthMode, float height, YGMeasureMode heightMode) override;

		RN::UI::ImageView *GetImageView() const { return _imageView; }

	private:
		RN::UI::ImageView *_imageView;
	};
} // namespace RN

#endif /* defined(__RN_FLEX_IMAGE_H_) */
