//
//  RNComputePass.h
//  Rayne
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_COMPUTEPASS_H__
#define __RAYNE_COMPUTEPASS_H__

#include "../Base/RNBase.h"
#include "RNFramePass.h"
#include "RNGPUBuffer.h"
#include "RNShader.h"
#include "RNTexture.h"

namespace RN
{
	class ComputePass : public FramePass
	{
	public:
		struct DispatchSize
		{
			uint32 x = 1;
			uint32 y = 1;
			uint32 z = 1;
		};

		class DispatchSnapshot
		{
		public:
			Shader *GetShader() const { return _shader.Get(); }
			const DispatchSize &GetGroupCount() const { return _groupCount; }
			RNAPI GPUBuffer *GetResourceBuffer(size_t nameHash) const;
			RNAPI Texture *GetResourceTexture(size_t nameHash) const;
			RNAPI const std::vector<uint8> *GetUniform(size_t nameHash) const;

		private:
			friend class ComputePass;

			void Reset();

			StrongRef<Shader> _shader;
			DispatchSize _groupCount;
			std::unordered_map<size_t, StrongRef<GPUBuffer>> _resourceBuffers;
			std::unordered_map<size_t, StrongRef<Texture>> _resourceTextures;
			std::unordered_map<size_t, std::vector<uint8>> _uniforms;
		};

		RNAPI ComputePass(Shader *shader = nullptr);
		RNAPI ~ComputePass() override;

		RNAPI void SetShader(Shader *shader);
		RNAPI void SetGroupCount(uint32 x, uint32 y = 1, uint32 z = 1);
		RNAPI void SetResourceBuffer(const String *name, GPUBuffer *buffer);
		RNAPI void SetResourceTexture(const String *name, Texture *texture);
		RNAPI void SetUniform(const String *name, const void *data, size_t size);

		Shader *GetShader() const { return _shader; }
		const DispatchSize &GetGroupCount() const { return _groupCount; }
		RNAPI void GetDispatchSnapshot(DispatchSnapshot &snapshot) const;

	private:
		Shader *_shader;
		DispatchSize _groupCount;
		std::unordered_map<size_t, StrongRef<GPUBuffer>> _resourceBuffers;
		std::unordered_map<size_t, StrongRef<Texture>> _resourceTextures;
		std::unordered_map<size_t, std::vector<uint8>> _uniforms;

		__RNDeclareMetaInternal(ComputePass)
	};
} // namespace RN

#endif /* __RAYNE_COMPUTEPASS_H__ */
