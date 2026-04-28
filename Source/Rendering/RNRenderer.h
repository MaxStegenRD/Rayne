//
//  RNRenderer.h
//  Rayne
//
//  Copyright 2015 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//


#ifndef __RAYNE_RENDERER_H_
#define __RAYNE_RENDERER_H_

#include "../Base/RNBase.h"
#include "../Math/RNColor.h"
#include "../Math/RNMatrix.h"
#include "../Math/RNQuaternion.h"
#include "../Math/RNVector.h"
#include "../Objects/RNDictionary.h"
#include "../Objects/RNWeakStorage.h"
#include "../Scene/RNCamera.h"
#include "../System/RNScreen.h"
#include "RNFramebuffer.h"
#include "RNGPUBuffer.h"
#include "RNMaterial.h"
#include "RNMesh.h"
#include "RNRendererTypes.h"
#include "RNShaderLibrary.h"
#include "RNSkeleton.h"
#include "RNTexture.h"
#include "RNWindow.h"

namespace RN
{
	class RenderPass;
	struct RenderPassResources;

	struct Drawable
	{
		Drawable()
		{
			renderGroup = 0xffff;
			_meshPipelineVersion = 0;
			_materialDrawSnapshotVersion = 0;
			_materialSnapshotVersion = 0;
			_skeletonDrawSnapshotVersion = 0;
			_transformNode = nullptr;
			_transformVersion = 0;
		}
		virtual ~Drawable()
		{}

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

			bool operator==(const PipelineKey &other) const
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
			bool operator!=(const PipelineKey &other) const { return !(*this == other); }
		};

		struct MergedMaterialSnapshot
		{
			void Update(const Drawable &drawable, Shader::UsageHint shaderHint, const Material::DrawSnapshot *overrideMaterial, uint64 overrideMaterialSnapshotVersion);

			bool isValid = false;
			uint64 materialSnapshotVersion = 0;
			uint64 overrideSnapshotVersion = 0;
			Shader::UsageHint shaderHint = Shader::UsageHint::Default;
			Shader *vertexShader = nullptr;
			Shader *fragmentShader = nullptr;
			Material::Properties properties;
			Material::PipelineProperties pipelineProperties;
		};

		void Update(Mesh *tmesh, Material *tmaterial, Skeleton *tskeleton, const SceneNode *node)
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

			if(node)
				Update(node);
		}
		virtual void Update(const SceneNode *node)
		{
			if(!node)
			{
				modelMatrix = Matrix();
				inverseModelMatrix = Matrix();
				_transformNode = nullptr;
				_transformVersion = 0;
			}
			else
			{
				renderGroup = node->GetRenderGroup();

				uint64 transformVersion = node->GetTransformVersion();
				if(_transformNode != node || _transformVersion != transformVersion)
				{
					modelMatrix = node->GetWorldTransform();
					inverseModelMatrix = node->GetInverseWorldTransform();
					_transformNode = node;
					_transformVersion = transformVersion;
				}
			}
		}
		void MakeDirty()
		{
			_meshPipelineVersion = 0;
			_materialDrawSnapshotVersion = 0;
			_skeletonDrawSnapshotVersion = 0;
		}

		// Source objects are kept for snapshot refresh/version checks.
		StrongRef<Mesh> _sourceMesh;
		StrongRef<Material> _sourceMaterial;
		StrongRef<Skeleton> _sourceSkeleton;

		// Captured on the main thread before submit, so encoding does not have to read mutable Mesh/Material/Skeleton state.
		Mesh::DrawSnapshot mesh;
		Material::DrawSnapshot material;
		Skeleton::DrawSnapshot skeleton;
		uint64 _meshPipelineVersion;
		uint64 _materialDrawSnapshotVersion;
		uint64 _materialSnapshotVersion;
		uint64 _skeletonDrawSnapshotVersion;
		Matrix modelMatrix;
		Matrix inverseModelMatrix;
		uint16 renderGroup;
		const SceneNode *_transformNode;
		uint64 _transformVersion;
	};

	class RendererDescriptor;
	class RenderingDevice;
	class Light;

	class Renderer : public Object
	{
	public:
		RNAPI static Renderer *GetActiveRenderer();
		RNAPI static bool IsHeadless();

		RNAPI ~Renderer();

		RNAPI virtual Window *CreateAWindow(const Vector2 &size, Screen *screen, const Window::SwapChainDescriptor &descriptor = Window::SwapChainDescriptor(), void *hwnd = nullptr) = 0;
		RNAPI virtual Window *GetMainWindow() = 0;
		RNAPI virtual void SetMainWindow(Window *window) = 0;

		RNAPI void Activate();
		RNAPI virtual void Deactivate();

		RNAPI virtual void Render(Function &&function) = 0;
		RNAPI virtual void SubmitCamera(Camera *camera, Function &&function) = 0;

		RNAPI virtual bool SupportsTextureFormat(const String *format) const = 0;
		RNAPI virtual bool SupportsDrawMode(DrawMode mode) const = 0;

		RNAPI virtual size_t GetAlignmentForType(PrimitiveType type) const = 0;
		RNAPI virtual size_t GetSizeForType(PrimitiveType type) const = 0;

		RNAPI virtual GPUBuffer *CreateBufferWithLength(size_t length, GPUResource::UsageOptions usageOptions, GPUResource::AccessOptions accessOptions, bool streameable) = 0;

		RNAPI virtual ShaderLibrary *CreateShaderLibraryWithFile(const String *file) = 0;
		RNAPI virtual ShaderLibrary *CreateShaderLibraryWithSource(const String *source) = 0;

		RNAPI virtual Shader *GetDefaultShader(Shader::Type type, Shader::Options *options, Shader::UsageHint shader = Shader::UsageHint::Default);
		RNAPI virtual ShaderLibrary *GetDefaultShaderLibrary() = 0;

		RNAPI virtual Texture *CreateTextureWithDescriptor(const Texture::Descriptor &descriptor) = 0;

		RNAPI virtual Framebuffer *CreateFramebuffer(const Vector2 &size) = 0;

		RNAPI virtual Drawable *CreateDrawable() = 0;
		RNAPI virtual void DeleteDrawable(Drawable *drawable) = 0;
		RNAPI virtual void SubmitDrawable(Drawable *drawable) = 0;
		RNAPI virtual RenderPassResources *CreateRenderPassResources();
		RNAPI virtual void DeleteRenderPassResources(RenderPassResources *resources);

		RNAPI virtual void WarmupDrawable(Mesh *mesh, Material *material, Camera *camera); //If the renderer supports it, this will create the necessary render pipeline state or similar to speed things up when actually rendering the first time

		RNAPI virtual void SubmitLight(const Light *light) = 0;
		
		RNAPI void PrintFrameStatistics(float interval = 5.0f);

		RendererDescriptor *GetDescriptor() const { return _descriptor; }
		RenderingDevice *GetDevice() const { return _device; }

	protected:
		RNAPI Renderer(RendererDescriptor *descriptor, RenderingDevice *device);
		
		struct CameraStatistics
		{
			uint64 numberOfDrawables;
			uint64 numberOfDrawCalls;
			uint64 numberOfVertices;
			uint64 numberOfIndices;
		};
		
		std::vector<CameraStatistics> _frameStatistics;
		double _frameStatisticsTimer;

	private:
		RenderingDevice *_device;
		RendererDescriptor *_descriptor;
		Dictionary *_defaultShaderCache[Shader::Type::COUNT][Shader::UsageHint::COUNT];
		Lockable _defaultShaderCacheLock;

		__RNDeclareMetaInternal(Renderer)
	};

	RNExceptionType(ShaderCompilation)
} // namespace RN


#endif /* __RAYNE_RENDERER_H_ */
