//
//  RNShadowRendererAttachment.cpp
//  Rayne
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNShadowRendererAttachment.h"
#include "RNRenderFrame.h"
#include "RNShadowPassSnapshot.h"

namespace RN
{
	RNDefineMeta(ShadowRendererAttachment, RendererAttachment)

	void ShadowRendererAttachment::RegisterShaderSources(Renderer *renderer)
	{
		RN_ASSERT(renderer, "Renderer mustn't be NULL");

		renderer->RegisterShaderSource(RNCSTR("directionalShadowTexture"), Shader::ArgumentTexture::Source::Pass);
		renderer->RegisterShaderSource(RNCSTR("directionalShadowMatrices"), Shader::UniformDescriptor::Source::Pass);
		renderer->RegisterShaderSource(RNCSTR("directionalShadowMatricesCount"), Shader::UniformDescriptor::Source::Pass);
		renderer->RegisterShaderSource(RNCSTR("directionalShadowInfo"), Shader::UniformDescriptor::Source::Pass);
	}

	void ShadowRendererAttachment::PrepareRenderFrame(Renderer *, RenderFrame &frame)
	{
		for(size_t i = 0; i < frame.GetPassCount(); i += 1)
		{
			PublishDirectionalShadowResources(frame, i);
		}
	}

	void ShadowRendererAttachment::PublishDirectionalShadowResources(RenderFrame &frame, size_t passIndex)
	{
		RenderFrame::Pass &pass = frame.GetPass(passIndex);
		ShadowPassSnapshot *snapshot = pass.GetAttachmentSnapshot<ShadowPassSnapshot>();
		bool hasSnapshot = snapshot && !snapshot->ContainsShadowCameraUID(pass.GetCameraSnapshot().GetSourceCameraUID());

		pass.SetPassResourceTexture(RNCSTR("directionalShadowTexture"), hasSnapshot ? snapshot->GetDirectionalShadowTexture() : nullptr);

		const std::vector<Matrix> *matrices = hasSnapshot ? &snapshot->GetDirectionalShadowMatrices() : nullptr;
		uint32 matrixCount = matrices ? static_cast<uint32>(matrices->size()) : 0;

		pass.SetPassUniform(RNCSTR("directionalShadowMatricesCount"), &matrixCount, sizeof(matrixCount));

		if(matrixCount > 0)
		{
			pass.SetPassUniform(RNCSTR("directionalShadowMatrices"), matrices->data(), sizeof(Matrix) * matrixCount);
		}
		else
		{
			float matrixData[16] = {};
			pass.SetPassUniform(RNCSTR("directionalShadowMatrices"), matrixData, sizeof(matrixData));
		}

		Vector2 shadowInfo = hasSnapshot ? snapshot->GetDirectionalShadowInfo() : Vector2();
		pass.SetPassUniform(RNCSTR("directionalShadowInfo"), &shadowInfo.x, sizeof(Vector2));
	}
}
