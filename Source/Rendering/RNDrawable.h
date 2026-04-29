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
	class Renderer;
	class SceneNode;

	struct Drawable
	{
		friend class Renderer;

		RNAPI Drawable();
		RNAPI virtual ~Drawable();

	private:
		struct MeshSnapshot
		{
			Mesh::DrawSnapshot _snapshot;
			uint64 _lastUsedFrameID = 0;
		};

		struct MaterialSnapshot
		{
			MaterialSnapshot(uint64 version) :
				_version(version)
			{}

			Material::DrawSnapshot _snapshot;
			uint64 _version = 0;
			uint64 _lastUsedFrameID = 0;
		};

		struct SkeletonSnapshot
		{
			Skeleton::DrawSnapshot _snapshot;
			uint64 _lastUsedFrameID = 0;
		};

	public:
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
			RNAPI void Update(const Material::DrawSnapshot &material, uint64 materialSnapshotVersion, Shader::UsageHint shaderHint, const Material::DrawSnapshot *overrideMaterialSnapshot, uint64 overrideMaterialSnapshotIdentity, uint64 overrideMaterialSnapshotVersion);
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
			uint64 _overrideSnapshotIdentity = 0;
			uint64 _overrideSnapshotVersion = 0;
			Shader::UsageHint _shaderHint = Shader::UsageHint::Default;
			Shader *_vertexShader = nullptr;
			Shader *_fragmentShader = nullptr;
			const Array *_textures = nullptr;
			Material::Properties _properties;
			Material::PipelineProperties _pipelineProperties;
		};

		class DrawSnapshotBundle
		{
		public:
			const Mesh::DrawSnapshot &GetMesh() const { return _mesh->_snapshot; }
			const Material::DrawSnapshot &GetMaterial() const { return _material->_snapshot; }
			const Skeleton::DrawSnapshot &GetSkeleton() const { return _skeleton->_snapshot; }
			uint64 GetMaterialSnapshotVersion() const { return _material->_version; }

		private:
			friend struct Drawable;

			DrawSnapshotBundle(MeshSnapshot *mesh, MaterialSnapshot *material, SkeletonSnapshot *skeleton) :
				_mesh(mesh),
				_material(material),
				_skeleton(skeleton)
			{}

			MeshSnapshot *_mesh;
			MaterialSnapshot *_material;
			SkeletonSnapshot *_skeleton;
		};

		RNAPI void Update(Mesh *tmesh, Material *tmaterial, Skeleton *tskeleton, const SceneNode *node);
		RNAPI virtual void Update(const SceneNode *node);
		RNAPI void MakeDirty();
		RNAPI void GetMeshBufferSnapshot(Mesh::BufferSnapshot &snapshot) const;
		RNAPI DrawSnapshotBundle GetDrawSnapshotBundleForFrame(uint64 frameID);

		const Matrix &GetModelMatrix() const { return _modelMatrix; }
		const Matrix &GetInverseModelMatrix() const { return _inverseModelMatrix; }
		uint16 GetRenderGroup() const { return _renderGroup; }

	private:
		void UpdateTransform(const SceneNode *node);
		bool DrainDrawSnapshots(uint64 completedFrameID);
		bool HasDrawSnapshotHistory() const;
		void UpdateDrawSnapshots();

		std::deque<MeshSnapshot> _meshSnapshots;
		std::deque<MaterialSnapshot> _materialSnapshots;
		std::deque<SkeletonSnapshot> _skeletonSnapshots;

		Matrix _modelMatrix;
		Matrix _inverseModelMatrix;
		uint16 _renderGroup = 0xffff;

		// Source objects are kept for snapshot refresh/version checks.
		StrongRef<Mesh> _sourceMesh;
		StrongRef<Material> _sourceMaterial;
		StrongRef<Skeleton> _sourceSkeleton;

		uint64 _meshPipelineVersion = 0;
		uint64 _materialDrawSnapshotVersion = 0;
		uint64 _materialSnapshotVersion = 0;
		uint64 _skeletonDrawSnapshotVersion = 0;
		uint64 _transformVersion = 0;

		bool _meshSnapshotDirty = true;
		bool _materialSnapshotDirty = true;
		bool _skeletonSnapshotDirty = true;
		bool _transformDirty = true;
		bool _isRegisteredForSnapshotDrain = false;

		const SceneNode *_transformNode = nullptr;
	};
} // namespace RN


#endif /* __RAYNE_DRAWABLE_H_ */
