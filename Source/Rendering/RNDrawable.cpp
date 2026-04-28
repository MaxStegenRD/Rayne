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
	Drawable::Drawable() = default;

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

	void Drawable::MergedMaterialSnapshot::Update(const Drawable &drawable, Shader::UsageHint hint, const Material::DrawSnapshot *overrideMaterialSnapshot, uint64 overrideMaterialSnapshotVersion)
	{
		Update(drawable.material, drawable._materialSnapshotVersion, hint, overrideMaterialSnapshot, overrideMaterialSnapshotVersion);
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
			_meshPipelineVersion = 0;
		}
		if(materialSourceChanged)
		{
			_sourceMaterial = tmaterial;
			_materialDrawSnapshotVersion = 0;
		}
		if(skeletonSourceChanged)
		{
			_sourceSkeleton = tskeleton;
			_skeletonDrawSnapshotVersion = 0;
		}

		if(tmesh)
		{
			uint64 pipelineVersion = tmesh->GetPipelineVersion();
			if(_meshPipelineVersion != pipelineVersion)
			{
				tmesh->GetDrawSnapshot(mesh);
				_meshPipelineVersion = pipelineVersion;
			}
		}
		else if(meshSourceChanged)
		{
			mesh.Reset();
			_meshPipelineVersion = 0;
		}

		if(tmaterial)
		{
			uint64 snapshotVersion = tmaterial->GetDrawSnapshotVersion();
			if(_materialDrawSnapshotVersion != snapshotVersion)
			{
				tmaterial->GetDrawSnapshot(material);
				_materialDrawSnapshotVersion = snapshotVersion;
				_materialSnapshotVersion += 1;
			}
		}
		else if(materialSourceChanged)
		{
			material.Reset();
			_materialDrawSnapshotVersion = 0;
			_materialSnapshotVersion += 1;
		}

		if(tskeleton)
		{
			uint64 snapshotVersion = tskeleton->GetDrawSnapshotVersion();
			if(_skeletonDrawSnapshotVersion != snapshotVersion)
			{
				tskeleton->GetDrawSnapshot(skeleton);
				_skeletonDrawSnapshotVersion = snapshotVersion;
			}
		}
		else if(skeletonSourceChanged)
		{
			skeleton.Reset();
			_skeletonDrawSnapshotVersion = 0;
		}

		Update(node);
	}

	void Drawable::Update(const SceneNode *node)
	{
		if(!node)
		{
			if(_transformNode)
			{
				_modelMatrix = Matrix();
				_inverseModelMatrix = Matrix();
				_renderGroup = 0xffff;
				_transformNode = nullptr;
				_transformVersion = 0;
			}

			return;
		}

		_renderGroup = node->GetRenderGroup();

		uint64 transformVersion = node->GetTransformVersion();
		if(_transformNode != node || _transformVersion != transformVersion)
		{
			_modelMatrix = node->GetWorldTransform();
			_inverseModelMatrix = node->GetInverseWorldTransform();
			_transformNode = node;
			_transformVersion = transformVersion;
		}
	}

	void Drawable::MakeDirty()
	{
		_meshPipelineVersion = 0;
		_materialDrawSnapshotVersion = 0;
		_skeletonDrawSnapshotVersion = 0;
	}
} // namespace RN
