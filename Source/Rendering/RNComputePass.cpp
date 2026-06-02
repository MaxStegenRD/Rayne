//
//  RNComputePass.cpp
//  Rayne
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNComputePass.h"

namespace RN
{
	RNDefineMeta(ComputePass, FramePass)

	GPUBuffer *ComputePass::DispatchSnapshot::GetResourceBuffer(size_t nameHash) const
	{
		auto iterator = _resourceBuffers.find(nameHash);
		return iterator == _resourceBuffers.end() ? nullptr : iterator->second.Get();
	}

	Texture *ComputePass::DispatchSnapshot::GetResourceTexture(size_t nameHash) const
	{
		auto iterator = _resourceTextures.find(nameHash);
		return iterator == _resourceTextures.end() ? nullptr : iterator->second.Get();
	}

	const std::vector<uint8> *ComputePass::DispatchSnapshot::GetUniform(size_t nameHash) const
	{
		auto iterator = _uniforms.find(nameHash);
		return iterator == _uniforms.end() ? nullptr : &iterator->second;
	}

	void ComputePass::DispatchSnapshot::Reset()
	{
		_shader = nullptr;
		_groupCount = DispatchSize();
		_groupOffset = DispatchOffset();
		_resourceBuffers.clear();
		_resourceTextures.clear();
		_uniforms.clear();
	}

	ComputePass::ComputePass(Shader *shader) :
		_shader(SafeRetain(shader))
	{}

	ComputePass::~ComputePass()
	{
		SafeRelease(_shader);
	}

	void ComputePass::SetShader(Shader *shader)
	{
		RN_ASSERT(!shader || shader->GetType() == Shader::Type::Compute, "ComputePass shader must be a compute shader");

		if(_shader == shader) return;

		SafeRelease(_shader);
		_shader = SafeRetain(shader);
	}

	void ComputePass::SetGroupCount(uint32 x, uint32 y, uint32 z)
	{
		RN_ASSERT(x > 0 && y > 0 && z > 0, "ComputePass group count must be greater than zero");

		if(_groupCount.x == x && _groupCount.y == y && _groupCount.z == z) return;

		_groupCount.x = x;
		_groupCount.y = y;
		_groupCount.z = z;
	}

	void ComputePass::SetGroupOffset(uint32 x, uint32 y, uint32 z)
	{
		if(_groupOffset.x == x && _groupOffset.y == y && _groupOffset.z == z) return;

		_groupOffset.x = x;
		_groupOffset.y = y;
		_groupOffset.z = z;
	}

	void ComputePass::SetResourceBuffer(const String *name, GPUBuffer *buffer)
	{
		RN_ASSERT(name, "ComputePass resource buffer name mustn't be NULL");

		size_t nameHash = name->GetHash();
		if(buffer) _resourceBuffers[nameHash] = buffer;
		else _resourceBuffers.erase(nameHash);
	}

	void ComputePass::SetResourceTexture(const String *name, Texture *texture)
	{
		RN_ASSERT(name, "ComputePass resource texture name mustn't be NULL");

		size_t nameHash = name->GetHash();
		if(texture) _resourceTextures[nameHash] = texture;
		else _resourceTextures.erase(nameHash);
	}

	void ComputePass::SetUniform(const String *name, const void *data, size_t size)
	{
		RN_ASSERT(name, "ComputePass uniform name mustn't be NULL");

		size_t nameHash = name->GetHash();
		if(data && size > 0)
		{
			const uint8 *bytes = static_cast<const uint8 *>(data);
			_uniforms[nameHash].assign(bytes, bytes + size);
		}
		else
		{
			_uniforms.erase(nameHash);
		}
	}

	void ComputePass::GetDispatchSnapshot(DispatchSnapshot &snapshot) const
	{
		snapshot.Reset();
		snapshot._shader = _shader;
		snapshot._groupCount = _groupCount;
		snapshot._groupOffset = _groupOffset;
		snapshot._resourceBuffers = _resourceBuffers;
		snapshot._resourceTextures = _resourceTextures;
		snapshot._uniforms = _uniforms;
	}
} // namespace RN
