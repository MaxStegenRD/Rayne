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

	void Drawable::MergedMaterialSnapshot::Update(const DrawPacket &drawPacket, Shader::UsageHint hint, const Material::DrawSnapshot *overrideMaterialSnapshot, uint64 overrideMaterialSnapshotVersion)
	{
		Update(drawPacket._material, drawPacket._materialSnapshotVersion, hint, overrideMaterialSnapshot, overrideMaterialSnapshotVersion);
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
		DrawPacket &drawPacket = GetMutableDrawPacket();

		bool meshSourceChanged = _sourceMesh.Get() != tmesh;
		bool materialSourceChanged = _sourceMaterial.Get() != tmaterial;
		bool skeletonSourceChanged = _sourceSkeleton.Get() != tskeleton;

		if(meshSourceChanged)
		{
			_sourceMesh = tmesh;
			_meshSourceSequence += 1;
		}
		if(materialSourceChanged)
		{
			_sourceMaterial = tmaterial;
			_materialSourceSequence += 1;
		}
		if(skeletonSourceChanged)
		{
			_sourceSkeleton = tskeleton;
			_skeletonSourceSequence += 1;
		}

		uint64 meshPipelineVersion = tmesh ? tmesh->GetPipelineVersion() : 0;
		if(tmesh)
		{
			if(drawPacket._meshSourceSequence != _meshSourceSequence || drawPacket._meshPipelineVersion != meshPipelineVersion)
			{
				tmesh->GetDrawSnapshot(drawPacket._mesh);
				drawPacket._meshSourceSequence = _meshSourceSequence;
				drawPacket._meshPipelineVersion = meshPipelineVersion;
			}
		}
		else if(drawPacket._meshSourceSequence != _meshSourceSequence || drawPacket._meshPipelineVersion != 0)
		{
			drawPacket._mesh.Reset();
			drawPacket._meshSourceSequence = _meshSourceSequence;
			drawPacket._meshPipelineVersion = 0;
		}

		uint64 materialDrawSnapshotVersion = tmaterial ? tmaterial->GetDrawSnapshotVersion() : 0;
		if(materialSourceChanged || _materialDrawSnapshotVersion != materialDrawSnapshotVersion)
		{
			_materialDrawSnapshotVersion = materialDrawSnapshotVersion;
			_materialSnapshotVersion += 1;
		}

		if(tmaterial)
		{
			if(drawPacket._materialSourceSequence != _materialSourceSequence || drawPacket._materialDrawSnapshotVersion != _materialDrawSnapshotVersion)
			{
				tmaterial->GetDrawSnapshot(drawPacket._material);
				drawPacket._materialSourceSequence = _materialSourceSequence;
				drawPacket._materialDrawSnapshotVersion = _materialDrawSnapshotVersion;
				drawPacket._materialSnapshotVersion = _materialSnapshotVersion;
			}
		}
		else if(drawPacket._materialSourceSequence != _materialSourceSequence || drawPacket._materialDrawSnapshotVersion != 0)
		{
			drawPacket._material.Reset();
			drawPacket._materialSourceSequence = _materialSourceSequence;
			drawPacket._materialDrawSnapshotVersion = 0;
			drawPacket._materialSnapshotVersion = _materialSnapshotVersion;
		}

		uint64 skeletonDrawSnapshotVersion = tskeleton ? tskeleton->GetDrawSnapshotVersion() : 0;
		if(tskeleton)
		{
			if(drawPacket._skeletonSourceSequence != _skeletonSourceSequence || drawPacket._skeletonDrawSnapshotVersion != skeletonDrawSnapshotVersion)
			{
				tskeleton->GetDrawSnapshot(drawPacket._skeleton);
				drawPacket._skeletonSourceSequence = _skeletonSourceSequence;
				drawPacket._skeletonDrawSnapshotVersion = skeletonDrawSnapshotVersion;
			}
		}
		else if(drawPacket._skeletonSourceSequence != _skeletonSourceSequence || drawPacket._skeletonDrawSnapshotVersion != 0)
		{
			drawPacket._skeleton.Reset();
			drawPacket._skeletonSourceSequence = _skeletonSourceSequence;
			drawPacket._skeletonDrawSnapshotVersion = 0;
		}

		Update(node);
	}

	void Drawable::Update(const SceneNode *node)
	{
		DrawPacket &drawPacket = GetMutableDrawPacket();
		if(_transformNode != node)
		{
			_transformNode = node;
			_transformSourceSequence += 1;
		}

		if(!node)
		{
			if(drawPacket._transformSourceSequence != _transformSourceSequence || drawPacket._transformVersion != 0 || drawPacket._renderGroup != 0xffff)
			{
				drawPacket._modelMatrix = Matrix();
				drawPacket._inverseModelMatrix = Matrix();
				drawPacket._renderGroup = 0xffff;
				drawPacket._transformSourceSequence = _transformSourceSequence;
				drawPacket._transformVersion = 0;
			}

			return;
		}

		drawPacket._renderGroup = node->GetRenderGroup();

		uint64 transformVersion = node->GetTransformVersion();
		if(drawPacket._transformSourceSequence != _transformSourceSequence || drawPacket._transformVersion != transformVersion)
		{
			drawPacket._modelMatrix = node->GetWorldTransform();
			drawPacket._inverseModelMatrix = node->GetInverseWorldTransform();
			drawPacket._transformSourceSequence = _transformSourceSequence;
			drawPacket._transformVersion = transformVersion;
		}
	}

	void Drawable::MakeDirty()
	{
		_meshSourceSequence += 1;
		_materialSourceSequence += 1;
		_materialDrawSnapshotVersion = 0;
		_skeletonSourceSequence += 1;
	}
} // namespace RN
