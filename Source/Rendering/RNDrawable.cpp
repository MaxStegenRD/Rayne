//
//  RNDrawable.cpp
//  Rayne
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNDrawable.h"
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

	void Drawable::MergedMaterialSnapshot::Update(const Material::DrawSnapshot &material, uint64 materialVersion, Shader::UsageHint hint, const Material::DrawSnapshot *overrideMaterialSnapshot, uint64 overrideMaterialSnapshotVersion)
	{
		if(_isValid && _materialSnapshotVersion == materialVersion && _overrideSnapshot == overrideMaterialSnapshot && _overrideSnapshotVersion == overrideMaterialSnapshotVersion && _shaderHint == hint)
			return;

		_materialSnapshotVersion = materialVersion;
		_overrideSnapshot = overrideMaterialSnapshot;
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

		if(_meshSnapshotDirty)
		{
			if(tmesh)
				tmesh->GetDrawSnapshot(_mesh);
			else
				_mesh.Reset();

			_meshPipelineVersion = meshPipelineVersion;
			_meshSnapshotDirty = false;
		}

		uint64 materialDrawSnapshotVersion = tmaterial ? tmaterial->GetDrawSnapshotVersion() : 0;
		if(materialSourceChanged || _materialDrawSnapshotVersion != materialDrawSnapshotVersion)
		{
			_materialDrawSnapshotVersion = materialDrawSnapshotVersion;
			_materialSnapshotVersion += 1;
			_materialSnapshotDirty = true;
		}

		if(_materialSnapshotDirty)
		{
			if(tmaterial)
				tmaterial->GetDrawSnapshot(_material);
			else
				_material.Reset();

			_materialSnapshotDirty = false;
		}

		uint64 skeletonDrawSnapshotVersion = tskeleton ? tskeleton->GetDrawSnapshotVersion() : 0;
		if(_skeletonDrawSnapshotVersion != skeletonDrawSnapshotVersion)
			_skeletonSnapshotDirty = true;

		if(_skeletonSnapshotDirty)
		{
			if(tskeleton)
				tskeleton->GetDrawSnapshot(_skeleton);
			else
				_skeleton.Reset();

			_skeletonDrawSnapshotVersion = skeletonDrawSnapshotVersion;
			_skeletonSnapshotDirty = false;
		}

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
} // namespace RN
