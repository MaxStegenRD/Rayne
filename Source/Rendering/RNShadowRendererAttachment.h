//
//  RNShadowRendererAttachment.h
//  Rayne
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_SHADOW_RENDERER_ATTACHMENT_H__
#define __RAYNE_SHADOW_RENDERER_ATTACHMENT_H__

#include "RNRenderer.h"

namespace RN
{
	class ShadowRendererAttachment : public RendererAttachment
	{
	public:
		RNAPI static void RegisterShaderSources(Renderer *renderer);
		RNAPI void PrepareRenderFrame(Renderer *renderer, RenderFrame &frame) override;

	private:
		void PublishDirectionalShadowResources(RenderFrame &frame, size_t passIndex);

		__RNDeclareMetaInternal(ShadowRendererAttachment)
	};
}

#endif /* __RAYNE_SHADOW_RENDERER_ATTACHMENT_H__ */
