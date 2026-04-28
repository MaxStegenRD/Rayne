//
//  RNRenderPassResources.cpp
//  Rayne
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNRenderPassResources.h"
#include "RNRenderer.h"

namespace RN
{
	RenderPassResources::RenderPassResources(Renderer *renderer) :
		_renderer(renderer)
	{}

	RenderPassResources::~RenderPassResources()
	{}

	void RenderPassResources::Delete()
	{
		_renderer->DeleteRenderPassResources(this);
	}

	void RenderPassResources::Update(RenderPass *renderPass, Material *effectiveOverrideMaterial)
	{
		uint64 snapshotVersion = renderPass->GetDrawSnapshotVersion();
		if(drawSnapshotVersion != snapshotVersion)
		{
			renderPass->GetDrawSnapshot(_drawSnapshot);
			drawSnapshotVersion = snapshotVersion;
		}
		else if(!_drawSnapshot.IsSubpass())
		{
			_drawSnapshot._framebuffer = renderPass->GetFramebuffer();
			_drawSnapshot._frame = renderPass->GetFrame();
		}
		else
		{
			_drawSnapshot._framebuffer = nullptr;
			_drawSnapshot._frame = Rect();
		}

		UpdateOverrideMaterial(effectiveOverrideMaterial);
	}

	void RenderPassResources::UpdateOverrideMaterial(Material *effectiveOverrideMaterial)
	{
		uint64 overrideSourceVersion = effectiveOverrideMaterial ? effectiveOverrideMaterial->GetDrawSnapshotVersion() : 0;
		if(overrideMaterialSource.Get() != effectiveOverrideMaterial || overrideMaterialSourceVersion != overrideSourceVersion)
		{
			overrideMaterialSource = effectiveOverrideMaterial;
			overrideMaterialSourceVersion = overrideSourceVersion;
			overrideMaterialSnapshotVersion += 1;
			if(effectiveOverrideMaterial)
				effectiveOverrideMaterial->GetDrawSnapshot(overrideMaterialSnapshot);
			else
				overrideMaterialSnapshot.Reset();
		}
	}
} // namespace RN
