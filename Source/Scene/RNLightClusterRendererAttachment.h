//
//  RNLightClusterRendererAttachment.h
//  Rayne
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_LIGHT_CLUSTER_RENDERER_ATTACHMENT_H__
#define __RAYNE_LIGHT_CLUSTER_RENDERER_ATTACHMENT_H__

#include "../Rendering/RNRenderer.h"

namespace RN
{
	class LightClusterRendererAttachment : public RendererAttachment
	{
	public:
		RNAPI static void RegisterShaderSources(Renderer *renderer);
		RNAPI void PrepareRenderFrame(Renderer *renderer, RenderFrame &frame) override;

	private:
		__RNDeclareMetaInternal(LightClusterRendererAttachment)
	};
}

#endif
