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

	const ComputePass::DispatchSize &ComputePass::DispatchSnapshot::GetGroupCount() const
	{
		static const DispatchSize defaultGroupCount;
		return _dispatchRegions.empty() ? defaultGroupCount : _dispatchRegions[0].groupCount;
	}

	const ComputePass::DispatchOffset &ComputePass::DispatchSnapshot::GetGroupOffset() const
	{
		static const DispatchOffset defaultGroupOffset;
		return _dispatchRegions.empty() ? defaultGroupOffset : _dispatchRegions[0].groupOffset;
	}

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
		_dispatchRegions.clear();
		_resourceBuffers.clear();
		_resourceTextures.clear();
		_uniforms.clear();
	}

	ComputePass::ComputePass(Shader *shader) :
		_shader(SafeRetain(shader))
	{
		_dispatchRegions.push_back(DispatchRegion());
	}

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

		DispatchRegion &region = GetPrimaryDispatchRegion();
		if(region.groupCount.x == x && region.groupCount.y == y && region.groupCount.z == z) return;

		region.groupCount.x = x;
		region.groupCount.y = y;
		region.groupCount.z = z;
	}

	void ComputePass::SetGroupOffset(uint32 x, uint32 y, uint32 z)
	{
		DispatchRegion &region = GetPrimaryDispatchRegion();
		if(region.groupOffset.x == x && region.groupOffset.y == y && region.groupOffset.z == z) return;

		region.groupOffset.x = x;
		region.groupOffset.y = y;
		region.groupOffset.z = z;
	}

	void ComputePass::AddDispatchRegion(uint32 groupCountX, uint32 groupCountY, uint32 groupCountZ, uint32 groupOffsetX, uint32 groupOffsetY, uint32 groupOffsetZ)
	{
		RN_ASSERT(groupCountX > 0 && groupCountY > 0 && groupCountZ > 0, "ComputePass dispatch region group count must be greater than zero");

		if(HasOnlyDefaultDispatchRegion())
			_dispatchRegions.clear();

		DispatchRegion region;
		region.groupCount.x = groupCountX;
		region.groupCount.y = groupCountY;
		region.groupCount.z = groupCountZ;
		region.groupOffset.x = groupOffsetX;
		region.groupOffset.y = groupOffsetY;
		region.groupOffset.z = groupOffsetZ;

		_dispatchRegions.push_back(region);
	}

	void ComputePass::ClearDispatchRegions()
	{
		_dispatchRegions.clear();
	}

	const ComputePass::DispatchSize &ComputePass::GetGroupCount() const
	{
		static const DispatchSize defaultGroupCount;
		return _dispatchRegions.empty() ? defaultGroupCount : _dispatchRegions[0].groupCount;
	}

	const ComputePass::DispatchOffset &ComputePass::GetGroupOffset() const
	{
		static const DispatchOffset defaultGroupOffset;
		return _dispatchRegions.empty() ? defaultGroupOffset : _dispatchRegions[0].groupOffset;
	}

	ComputePass::DispatchRegion &ComputePass::GetPrimaryDispatchRegion()
	{
		if(_dispatchRegions.empty())
			_dispatchRegions.push_back(DispatchRegion());

		return _dispatchRegions[0];
	}

	bool ComputePass::HasOnlyDefaultDispatchRegion() const
	{
		if(_dispatchRegions.size() != 1) return false;

		const DispatchRegion &region = _dispatchRegions[0];
		return region.groupCount.x == 1 && region.groupCount.y == 1 && region.groupCount.z == 1 && region.groupOffset.x == 0 && region.groupOffset.y == 0 && region.groupOffset.z == 0;
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
		snapshot._dispatchRegions = _dispatchRegions;
		snapshot._resourceBuffers = _resourceBuffers;
		snapshot._resourceTextures = _resourceTextures;
		snapshot._uniforms = _uniforms;
	}
} // namespace RN
