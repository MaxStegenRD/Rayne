//
//  RNDrawable.cpp
//  Rayne
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNDrawable.h"
#include "RNRenderingConfig.h"
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
		_textures = material.GetTextures();
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

	void Drawable::Update(Mesh *tmesh, Material *tmaterial, Skeleton *tskeleton, const SceneNode *node)
	{
		bool meshSourceChanged = _sourceMesh.Get() != tmesh;
		bool materialSourceChanged = _sourceMaterial.Get() != tmaterial;
		bool skeletonSourceChanged = _sourceSkeleton.Get() != tskeleton;

		if(meshSourceChanged)
		{
			_sourceMesh = tmesh;
			_meshSnapshotDirty = true;
		}
		if(materialSourceChanged)
		{
			_sourceMaterial = tmaterial;
			_materialSnapshotDirty = true;
		}
		if(skeletonSourceChanged)
		{
			_sourceSkeleton = tskeleton;
			_skeletonSnapshotDirty = true;
		}

		uint64 meshPipelineVersion = tmesh ? tmesh->GetPipelineVersion() : 0;
		if(_meshPipelineVersion != meshPipelineVersion)
			_meshSnapshotDirty = true;

		uint64 materialDrawSnapshotVersion = tmaterial ? tmaterial->GetDrawSnapshotVersion() : 0;
		if(materialSourceChanged || _materialDrawSnapshotVersion != materialDrawSnapshotVersion)
		{
			_materialDrawSnapshotVersion = materialDrawSnapshotVersion;
			_materialSnapshotVersion += 1;
			_materialSnapshotDirty = true;
		}

		uint64 skeletonDrawSnapshotVersion = tskeleton ? tskeleton->GetDrawSnapshotVersion() : 0;
		if(_skeletonDrawSnapshotVersion != skeletonDrawSnapshotVersion)
			_skeletonSnapshotDirty = true;

		UpdateTransform(node);
	}

	void Drawable::Update(const SceneNode *node)
	{
		UpdateTransform(node);
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
			if(_transformDirty || _transformVersion != 0 || _renderGroup != 0xffff)
			{
				_modelMatrix = Matrix();
				_inverseModelMatrix = Matrix();
				_renderGroup = 0xffff;
				_transformVersion = 0;
				_transformDirty = false;
			}

			return;
		}

		_renderGroup = node->GetRenderGroup();

		uint64 transformVersion = node->GetTransformVersion();
		if(_transformDirty || _transformVersion != transformVersion)
		{
			_modelMatrix = node->GetWorldTransform();
			_inverseModelMatrix = node->GetInverseWorldTransform();
			_transformVersion = transformVersion;
			_transformDirty = false;
		}
	}

	void Drawable::MakeDirty()
	{
		_meshSnapshotDirty = true;
		_materialSnapshotDirty = true;
		_materialSnapshotVersion += 1;
		_skeletonSnapshotDirty = true;
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
		if(_meshSnapshots.empty() || _meshSnapshotDirty || _materialSnapshotDirty || _skeletonSnapshotDirty)
			UpdateDrawSnapshots();

		RN_DEBUG_ASSERT(!_meshSnapshots.empty() && !_materialSnapshots.empty() && !_skeletonSnapshots.empty(), "Drawable has no draw snapshots");

		DrawSnapshotBundle snapshot(&_meshSnapshots.back(), &_materialSnapshots.back(), &_skeletonSnapshots.back());
		snapshot._mesh->_lastUsedFrameID = frameID;
		snapshot._material->_lastUsedFrameID = frameID;
		snapshot._skeleton->_lastUsedFrameID = frameID;

		if(_meshSnapshots.size() > 1 || _materialSnapshots.size() > 1 || _skeletonSnapshots.size() > 1)
		{
			// A frame can be on the render thread while queued submissions wait behind it.
			// Keep enough snapshot history for that whole in-flight window.
			uint64 completedFrameID = 0;
			if(frameID > RN_RENDERING_FRAME_SUBMISSION_QUEUE_SIZE + 1)
				completedFrameID = frameID - RN_RENDERING_FRAME_SUBMISSION_QUEUE_SIZE - 1;
			DrainDrawSnapshots(completedFrameID);
		}

		return snapshot;
	}

	void Drawable::DrainDrawSnapshots(uint64 completedFrameID)
	{
		while(_meshSnapshots.size() > 1 && _meshSnapshots.front()._lastUsedFrameID <= completedFrameID)
			_meshSnapshots.pop_front();

		while(_materialSnapshots.size() > 1 && _materialSnapshots.front()._lastUsedFrameID <= completedFrameID)
			_materialSnapshots.pop_front();

		while(_skeletonSnapshots.size() > 1 && _skeletonSnapshots.front()._lastUsedFrameID <= completedFrameID)
			_skeletonSnapshots.pop_front();
	}

	void Drawable::UpdateDrawSnapshots()
	{
		if(_meshSnapshotDirty || _meshSnapshots.empty())
		{
			_meshSnapshots.emplace_back();
			MeshSnapshot &snapshot = _meshSnapshots.back();

			Mesh *mesh = _sourceMesh.Get();
			if(mesh)
				mesh->GetDrawSnapshot(snapshot._snapshot);
			else
				snapshot._snapshot.Reset();

			_meshPipelineVersion = mesh ? mesh->GetPipelineVersion() : 0;
			_meshSnapshotDirty = false;
		}

		if(_materialSnapshotDirty || _materialSnapshots.empty())
		{
			_materialSnapshots.emplace_back(_materialSnapshotVersion);
			MaterialSnapshot &snapshot = _materialSnapshots.back();

			Material *material = _sourceMaterial.Get();
			if(material)
				material->GetDrawSnapshot(snapshot._snapshot);
			else
				snapshot._snapshot.Reset();

			_materialDrawSnapshotVersion = material ? material->GetDrawSnapshotVersion() : 0;
			_materialSnapshotDirty = false;
		}

		if(_skeletonSnapshotDirty || _skeletonSnapshots.empty())
		{
			_skeletonSnapshots.emplace_back();
			SkeletonSnapshot &snapshot = _skeletonSnapshots.back();

			Skeleton *skeleton = _sourceSkeleton.Get();
			if(skeleton)
				skeleton->GetDrawSnapshot(snapshot._snapshot);
			else
				snapshot._snapshot.Reset();

			_skeletonDrawSnapshotVersion = skeleton ? skeleton->GetDrawSnapshotVersion() : 0;
			_skeletonSnapshotDirty = false;
		}
	}
} // namespace RN
