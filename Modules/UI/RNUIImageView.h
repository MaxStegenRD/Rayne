//
//  RNUIImageView.h
//  Rayne
//
//  Copyright 2017 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_UIIMAGEVIEW_H_
#define __RAYNE_UIIMAGEVIEW_H_

#include "RNUIView.h"

namespace RN
{
	namespace UI
	{
		class ImageView : public View
		{
		public:
			UIAPI ImageView();
			UIAPI ImageView(Texture *image);
			UIAPI ImageView(Framebuffer *framebuffer);
			UIAPI ~ImageView();

			UIAPI void SetFramebuffer(Framebuffer *framebuffer);
			UIAPI void SetImage(Texture *image);
			Texture *GetImage() const { return _image; }
			Framebuffer *GetFramebuffer() const { return _framebuffer; }

			UIAPI void SetColor(Color color); //Multiplicative, including alpha!
			UIAPI void SetOutline(Color color, float thickness) override;
			UIAPI void SetImageMaterial(Material *material);

		protected:
			UIAPI void UpdateModel() override;
			UIAPI void SetOpacityFromParent(float parentCombinedOpacity) override;

		private:
			Framebuffer *_framebuffer;
			Texture *_image;
			Color _color;
			Material *_imageMaterial;

			RNDeclareMetaAPI(ImageView, UIAPI)
		};
	} // namespace UI
} // namespace RN


#endif /* __RAYNE_UIIMAGEVIEW_H_ */
