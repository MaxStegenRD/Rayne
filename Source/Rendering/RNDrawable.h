//
//  RNDrawable.h
//  Rayne
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//


#ifndef __RAYNE_DRAWABLE_H_
#define __RAYNE_DRAWABLE_H_

#include "../Base/RNBase.h"
#include "../Math/RNMatrix.h"
#include "../Objects/RNObject.h"
#include "RNFramebuffer.h"
#include "RNMaterial.h"
#include "RNMesh.h"
#include "RNSkeleton.h"

namespace RN
{
	class RenderPass;
	class SceneNode;

	struct Drawable
	{
		RNAPI Drawable();
		RNAPI virtual ~Drawable();

		struct PipelineKey
		{
			size_t meshPipelineHash = 0;
			Framebuffer *framebuffer = nullptr;
			Shader *vertexShader = nullptr;
			Shader *fragmentShader = nullptr;
			Material::PipelineProperties materialProperties;
			RenderPass *renderPass = nullptr;
			uint64 renderPassSignature = 0;
			uint8 renderViewCount = 0;
			uint32 subpassIndex = 0;

			RNAPI bool operator==(const PipelineKey &other) const;
			bool operator!=(const PipelineKey &other) const { return !(*this == other); }
		};

		struct MergedMaterialSnapshot
		{
			RNAPI void Update(const Drawable &drawable, Shader::UsageHint shaderHint, const Material::DrawSnapshot *overrideMaterialSnapshot, uint64 overrideMaterialSnapshotVersion);
			RNAPI void Update(const Material::DrawSnapshot &material, uint64 materialSnapshotVersion, Shader::UsageHint shaderHint, const Material::DrawSnapshot *overrideMaterialSnapshot, uint64 overrideMaterialSnapshotVersion);
			RNAPI bool IsTextureSetEqual(const MergedMaterialSnapshot &other) const;
			RNAPI bool IsTextureSetEqualLite(const MergedMaterialSnapshot &other) const;
			Shader *GetVertexShader() const { return _vertexShader; }
			Shader *GetFragmentShader() const { return _fragmentShader; }
			const Array *GetTextures() const { return _textures; }
			const Material::Properties &GetProperties() const { return _properties; }
			const Material::PipelineProperties &GetPipelineProperties() const { return _pipelineProperties; }

		private:
			bool _isValid = false;
			uint64 _materialSnapshotVersion = 0;
			const Material::DrawSnapshot *_overrideSnapshot = nullptr;
			uint64 _overrideSnapshotVersion = 0;
			Shader::UsageHint _shaderHint = Shader::UsageHint::Default;
			Shader *_vertexShader = nullptr;
			Shader *_fragmentShader = nullptr;
			const Array *_textures = nullptr;
			Material::Properties _properties;
			Material::PipelineProperties _pipelineProperties;
		};

		RNAPI void Update(Mesh *tmesh, Material *tmaterial, Skeleton *tskeleton, const SceneNode *node);
		RNAPI virtual void Update(const SceneNode *node);
		RNAPI void MakeDirty();

		// Captured on the main thread before submit, so encoding does not have to read mutable Mesh/Material/Skeleton state.
		Mesh::DrawSnapshot mesh;
		Material::DrawSnapshot material;
		Skeleton::DrawSnapshot skeleton;
		Matrix modelMatrix;
		Matrix inverseModelMatrix;
		uint16 renderGroup;

	private:
		// Source objects are kept for snapshot refresh/version checks.
		StrongRef<Mesh> _sourceMesh;
		StrongRef<Material> _sourceMaterial;
		StrongRef<Skeleton> _sourceSkeleton;

		uint64 _meshPipelineVersion;
		uint64 _materialDrawSnapshotVersion;
		uint64 _materialSnapshotVersion;
		uint64 _skeletonDrawSnapshotVersion;
		const SceneNode *_transformNode;
		uint64 _transformVersion;
	};
} // namespace RN


#endif /* __RAYNE_DRAWABLE_H_ */
