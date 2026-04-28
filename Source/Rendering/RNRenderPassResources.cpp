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

	const RenderPassResources::DrawPacket &RenderPassResources::GetDrawPacket(uint8 packetSlot) const
	{
		RN_DEBUG_ASSERT(packetSlot < RN_RENDERING_PACKET_SLOT_COUNT, "Invalid render pass packet slot");
		return _drawPackets[packetSlot];
	}

	RenderPassResources::DrawPacket &RenderPassResources::GetUpdateDrawPacket()
	{
		RN_ASSERT(_renderer, "RenderPassResources needs a renderer to update draw packets");
		uint8 updatePacketSlot = _renderer->GetUpdatePacketSlot();
		RN_DEBUG_ASSERT(updatePacketSlot < RN_RENDERING_PACKET_SLOT_COUNT, "Invalid render pass packet slot");
		return _drawPackets[updatePacketSlot];
	}

	void RenderPassResources::Update(RenderPass *renderPass, Material *effectiveOverrideMaterial)
	{
		DrawPacket &drawPacket = GetUpdateDrawPacket();
		uint64 snapshotVersion = renderPass->GetDrawSnapshotVersion();
		if(drawPacket._drawSnapshotSourceVersion != snapshotVersion)
		{
			renderPass->GetDrawSnapshot(drawPacket._drawSnapshot);
			drawPacket._drawSnapshotSourceVersion = snapshotVersion;
		}
		else if(!drawPacket._drawSnapshot.IsSubpass())
		{
			drawPacket._drawSnapshot._framebuffer = renderPass->GetFramebuffer();
			drawPacket._drawSnapshot._frame = renderPass->GetFrame();
		}
		else
		{
			drawPacket._drawSnapshot._framebuffer = nullptr;
			drawPacket._drawSnapshot._frame = Rect();
		}

		UpdateOverrideMaterial(effectiveOverrideMaterial, drawPacket);
	}

	void RenderPassResources::UpdateOverrideMaterial(Material *effectiveOverrideMaterial, DrawPacket &drawPacket)
	{
		bool overrideMaterialSourceChanged = overrideMaterialSource.Get() != effectiveOverrideMaterial;
		uint64 overrideSourceVersion = effectiveOverrideMaterial ? effectiveOverrideMaterial->GetDrawSnapshotVersion() : 0;
		if(overrideMaterialSourceChanged)
		{
			overrideMaterialSource = effectiveOverrideMaterial;
			overrideMaterialSourceSequence += 1;
		}

		if(overrideMaterialSourceChanged || overrideMaterialSourceVersion != overrideSourceVersion)
		{
			overrideMaterialSourceVersion = overrideSourceVersion;
			overrideMaterialSnapshotVersion += 1;
		}

		if(drawPacket._overrideMaterialSourceSequence != overrideMaterialSourceSequence || drawPacket._overrideMaterialSourceVersion != overrideMaterialSourceVersion)
		{
			drawPacket._hasOverrideMaterial = effectiveOverrideMaterial != nullptr;
			drawPacket._overrideMaterialSourceSequence = overrideMaterialSourceSequence;
			drawPacket._overrideMaterialSourceVersion = overrideMaterialSourceVersion;
			drawPacket._overrideMaterialSnapshotVersion = overrideMaterialSnapshotVersion;

			if(effectiveOverrideMaterial)
				effectiveOverrideMaterial->GetDrawSnapshot(drawPacket._overrideMaterialSnapshot);
			else
				drawPacket._overrideMaterialSnapshot.Reset();
		}
	}
} // namespace RN
