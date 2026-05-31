//
//  RNDrawable.cpp
//  Rayne
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNDrawable.h"
#include "RNRenderer.h"
#include "../Scene/RNSceneNode.h"

namespace RN
{
	Drawable::Drawable()
	{}

	Drawable::~Drawable() = default;

	bool Drawable::PipelineKey::operator==(const PipelineKey &other) const
	{
		return meshPipelineHash == other.meshPipelineHash &&
			framebuffer == other.framebuffer &&
			vertexShader == other.vertexShader &&
			fragmentShader == other.fragmentShader &&
			materialProperties == other.materialProperties &&
			renderPass == other.renderPass &&
			renderPassSignature == other.renderPassSignature &&
			renderViewCount == other.renderViewCount &&
			subpassIndex == other.subpassIndex;
	}

	void Drawable::MergedMaterialSnapshot::Update(const Material::DrawSnapshot &material, uint64 materialVersion, Shader::UsageHint hint, const Material::DrawSnapshot *overrideMaterialSnapshot, uint64 overrideMaterialSnapshotIdentity, uint64 overrideMaterialSnapshotVersion)
	{
		if(_isValid && _materialSnapshotVersion == materialVersion && _overrideSnapshotIdentity == overrideMaterialSnapshotIdentity && _overrideSnapshotVersion == overrideMaterialSnapshotVersion && _shaderHint == hint)
			return;

		_materialSnapshotVersion = materialVersion;
		_overrideSnapshotIdentity = overrideMaterialSnapshotIdentity;
		_overrideSnapshotVersion = overrideMaterialSnapshotVersion;
		_shaderHint = hint;
		_vertexShader = material.GetSelectedVertexShader(hint, overrideMaterialSnapshot);
		_fragmentShader = material.GetSelectedFragmentShader(hint, overrideMaterialSnapshot);
		const Array *overrideTextures = overrideMaterialSnapshot ? overrideMaterialSnapshot->GetTextures() : nullptr;
		_textures = (overrideTextures && overrideTextures->GetCount() > 0) ? overrideTextures : material.GetTextures();
		material.GetMergedProperties(overrideMaterialSnapshot, _properties);
		material.GetMergedPipelineProperties(overrideMaterialSnapshot, _pipelineProperties);
		_isValid = true;
	}

	bool Drawable::MergedMaterialSnapshot::IsTextureSetEqual(const MergedMaterialSnapshot &other) const
	{
		if(_textures == other._textures) return true;
		if(!_textures || !other._textures) return false;

		return _textures->IsEqual(other._textures);
	}

	bool Drawable::MergedMaterialSnapshot::IsTextureSetEqualLite(const MergedMaterialSnapshot &other) const
	{
		if(_textures == other._textures) return true;
		if(!_textures || !other._textures) return false;

		return _textures->IsEqualLite(other._textures);
	}

	void Drawable::SetSources(Mesh *mesh, Material *material, Skeleton *skeleton)
	{
		bool meshSourceChanged = _sourceMesh.Get() != mesh;
		bool materialSourceChanged = _sourceMaterial.Get() != material;
		bool skeletonSourceChanged = _sourceSkeleton.Get() != skeleton;

		if(meshSourceChanged)
		{
			_sourceMesh = mesh;
			_drawSnapshotDirtyMask |= MeshSnapshotDirty;
		}
		if(materialSourceChanged)
		{
			_sourceMaterial = material;
			_materialDrawSnapshotVersion = material ? material->GetDrawSnapshotVersion() : 0;
			_materialSnapshotVersion += 1;
			_drawSnapshotDirtyMask |= MaterialSnapshotDirty;
		}
		if(skeletonSourceChanged)
		{
			_sourceSkeleton = skeleton;
			_drawSnapshotDirtyMask |= SkeletonSnapshotDirty;
		}
	}

	void Drawable::UpdateTransform(const SceneNode *node)
	{
		if(_transformNode != node)
		{
			_transformNode = node;
			_transformDirty = true;
		}

		if(!node)
		{
			if(_transformDirty || _transformVersion != 0)
			{
				_modelMatrix = Matrix();
				_inverseModelMatrix = Matrix();
				_transformVersion = 0;
				_transformDirty = false;
			}

			return;
		}

		uint64 transformVersion = node->GetTransformVersion();
		if(_transformDirty || _transformVersion != transformVersion)
		{
			_modelMatrix = node->GetWorldTransform();
			_inverseModelMatrix = node->GetInverseWorldTransform();
			_transformVersion = transformVersion;
			_transformDirty = false;
		}
	}

	void Drawable::UpdateSourceVersions()
	{
		Mesh *mesh = _sourceMesh.Get();
		uint64 meshPipelineVersion = mesh ? mesh->GetPipelineVersion() : 0;
		if(_meshPipelineVersion != meshPipelineVersion)
			_drawSnapshotDirtyMask |= MeshSnapshotDirty;

		Material *material = _sourceMaterial.Get();
		uint64 materialDrawSnapshotVersion = material ? material->GetDrawSnapshotVersion() : 0;
		if(_materialDrawSnapshotVersion != materialDrawSnapshotVersion)
		{
			_materialDrawSnapshotVersion = materialDrawSnapshotVersion;
			_materialSnapshotVersion += 1;
			_drawSnapshotDirtyMask |= MaterialSnapshotDirty;
		}

		Skeleton *skeleton = _sourceSkeleton.Get();
		uint64 skeletonDrawSnapshotVersion = skeleton ? skeleton->GetDrawSnapshotVersion() : 0;
		if(_skeletonDrawSnapshotVersion != skeletonDrawSnapshotVersion)
			_drawSnapshotDirtyMask |= SkeletonSnapshotDirty;
	}

	void Drawable::GetMeshBufferSnapshot(Mesh::BufferSnapshot &snapshot) const
	{
		Mesh *mesh = _sourceMesh.Get();
		if(mesh)
			mesh->GetBufferSnapshot(snapshot);
		else
			snapshot.Reset();
	}

	Drawable::DrawSnapshotBundle Drawable::GetDrawSnapshotBundleForFrame(uint64 frameID)
	{
		UpdateSourceVersions();

		if(_drawSnapshotDirtyMask != 0)
			UpdateDrawSnapshots(frameID);

		RN_DEBUG_ASSERT(!_meshSnapshots.empty() && !_materialSnapshots.empty() && !_skeletonSnapshots.empty(), "Drawable has no draw snapshots");

		return DrawSnapshotBundle(&_meshSnapshots.back(), &_materialSnapshots.back(), &_skeletonSnapshots.back());
	}

	bool Drawable::DrainDrawSnapshots(uint64 completedFrameID)
	{
		while(_meshSnapshots.size() > 1 && _meshSnapshots.front()._lastUsedFrameID <= completedFrameID)
			_meshSnapshots.pop_front();

		while(_materialSnapshots.size() > 1 && _materialSnapshots.front()._lastUsedFrameID <= completedFrameID)
			_materialSnapshots.pop_front();

		while(_skeletonSnapshots.size() > 1 && _skeletonSnapshots.front()._lastUsedFrameID <= completedFrameID)
			_skeletonSnapshots.pop_front();

		return HasDrawSnapshotHistory();
	}

	bool Drawable::HasDrawSnapshotHistory() const
	{
		return _meshSnapshots.size() > 1 || _materialSnapshots.size() > 1 || _skeletonSnapshots.size() > 1;
	}

	void Drawable::UpdateDrawSnapshots(uint64 frameID)
	{
		bool didAddSnapshot = false;

		if((_drawSnapshotDirtyMask & MeshSnapshotDirty) != 0)
		{
			if(!_meshSnapshots.empty())
				_meshSnapshots.back()._lastUsedFrameID = frameID;

			_meshSnapshots.emplace_back();
			didAddSnapshot = true;
			MeshSnapshot &snapshot = _meshSnapshots.back();

			Mesh *mesh = _sourceMesh.Get();
			if(mesh)
				mesh->GetDrawSnapshot(snapshot._snapshot);
			else
				snapshot._snapshot.Reset();

			_meshPipelineVersion = mesh ? mesh->GetPipelineVersion() : 0;
			_drawSnapshotDirtyMask &= AllSnapshotsDirty ^ MeshSnapshotDirty;
		}

		if((_drawSnapshotDirtyMask & MaterialSnapshotDirty) != 0)
		{
			if(!_materialSnapshots.empty())
				_materialSnapshots.back()._lastUsedFrameID = frameID;

			_materialSnapshots.emplace_back(_materialSnapshotVersion);
			didAddSnapshot = true;
			MaterialSnapshot &snapshot = _materialSnapshots.back();

			Material *material = _sourceMaterial.Get();
			if(material)
				material->GetDrawSnapshot(snapshot._snapshot);
			else
				snapshot._snapshot.Reset();

			_materialDrawSnapshotVersion = material ? material->GetDrawSnapshotVersion() : 0;
			_drawSnapshotDirtyMask &= AllSnapshotsDirty ^ MaterialSnapshotDirty;
		}

		if((_drawSnapshotDirtyMask & SkeletonSnapshotDirty) != 0)
		{
			if(!_skeletonSnapshots.empty())
				_skeletonSnapshots.back()._lastUsedFrameID = frameID;

			_skeletonSnapshots.emplace_back();
			didAddSnapshot = true;
			SkeletonSnapshot &snapshot = _skeletonSnapshots.back();

			Skeleton *skeleton = _sourceSkeleton.Get();
			if(skeleton)
				skeleton->GetDrawSnapshot(snapshot._snapshot);
			else
				snapshot._snapshot.Reset();

			_skeletonDrawSnapshotVersion = skeleton ? skeleton->GetDrawSnapshotVersion() : 0;
			_drawSnapshotDirtyMask &= AllSnapshotsDirty ^ SkeletonSnapshotDirty;
		}

		if(didAddSnapshot && HasDrawSnapshotHistory() && !Renderer::IsHeadless())
			Renderer::GetActiveRenderer()->RegisterDrawableForSnapshotDrain(this);
	}
} // namespace RN
