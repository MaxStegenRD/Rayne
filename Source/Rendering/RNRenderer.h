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
#include "RNDrawable.h"
#include "RNFramebuffer.h"
#include "RNGPUBuffer.h"
#include "RNMaterial.h"
#include "RNMesh.h"
#include "RNRenderingConfig.h"
#include "RNRendererTypes.h"
#include "RNShaderLibrary.h"
#include "RNSkeleton.h"
#include "RNTexture.h"
#include "RNWindow.h"

namespace RN
{
	class RenderFrame;
	struct RenderPassResources;

	class RendererDescriptor;
	class RenderingDevice;
	class Light;
	class Renderer;
	class RendererAttachment : public Object
	{
	public:
		RNAPI virtual void PrepareRenderFrame(Renderer *renderer, RenderFrame &frame);

	protected:
		RNAPI RendererAttachment();
		RNAPI ~RendererAttachment() override;

		__RNDeclareMetaInternal(RendererAttachment)
	};

	class Renderer : public Object
	{
		friend struct Drawable;

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
		RNAPI virtual void ScheduleRenderThreadWork(Function &&function);
		RNAPI virtual void SynchronizeRenderThread();
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
		RNAPI virtual void SubmitDrawable(Drawable *drawable, const SceneNode *node) = 0;
		RNAPI virtual RenderPassResources *CreateRenderPassResources();
		RNAPI virtual void DeleteRenderPassResources(RenderPassResources *resources);

		RNAPI virtual void WarmupDrawable(Mesh *mesh, Material *material, Camera *camera); //If the renderer supports it, this will create the necessary render pipeline state or similar to speed things up when actually rendering the first time

		RNAPI virtual void SubmitLight(const Light *light) = 0;

		RNAPI void AddAttachment(RendererAttachment *attachment);
		RNAPI void RemoveAttachment(RendererAttachment *attachment);
		RNAPI void SubmitAttachmentSnapshot(Object *snapshot);
		RNAPI void SubmitPassAttachmentSnapshot(size_t passIndex, Object *snapshot);

		RNAPI void RegisterArgumentSource(const String *name, Shader::ArgumentBuffer::Source source);
		RNAPI void RegisterArgumentSource(const String *name, Shader::ArgumentTexture::Source source);
		RNAPI Shader::ArgumentBuffer::Source GetArgumentSource(const String *name, Shader::ArgumentBuffer::Source defaultSource) const;
		RNAPI Shader::ArgumentTexture::Source GetArgumentSource(const String *name, Shader::ArgumentTexture::Source defaultSource) const;
		
		RendererDescriptor *GetDescriptor() const { return _descriptor; }
		RenderingDevice *GetDevice() const { return _device; }

	protected:
		RNAPI Renderer(RendererDescriptor *descriptor, RenderingDevice *device);
		RNAPI void BeginRenderFrameSubmission(RenderFrame &frame);
		RNAPI void FinishRenderFrameSubmission(const RenderFrame &frame);
		RNAPI RenderFrame *SetActiveRenderFrame(RenderFrame *frame);
		RNAPI void PrepareRendererAttachments(RenderFrame &frame);
		RNAPI void QueueDrawableDeletion(Drawable *drawable);
		RNAPI void FlushAllDeletedDrawables();
		RNAPI void PrintFrameStatistics(const RenderFrame &frame, float interval = RN_RENDERING_FRAME_STATISTICS_INTERVAL);

	private:
		struct DeletedDrawable
		{
			Drawable *drawable;
			uint64 frameID;
		};

		void RegisterDrawableForSnapshotDrain(Drawable *drawable);
		void UnregisterDrawableFromSnapshotDrain(Drawable *drawable);
		void DrainDrawableSnapshots(uint64 completedFrameID);
		void FlushDeletedDrawables();
#if RN_BUILD_DEBUG
		void TrackArgumentSourceName(size_t nameHash, const String *name) const;
#endif

		double _frameStatisticsTimer;
		uint64 _lastStartedRenderFrameID;
		uint64 _completedRenderFrameID;
		size_t _lastRenderFrameDrawItemCount;
		std::vector<DeletedDrawable> _pendingDeletedDrawables;
		std::vector<Drawable *> _drawablesPendingSnapshotDrain;
		std::vector<StrongRef<RendererAttachment>> _rendererAttachments;
		std::unordered_map<size_t, Shader::ArgumentBuffer::Source> _argumentBufferSources;
		std::unordered_map<size_t, Shader::ArgumentTexture::Source> _argumentTextureSources;
#if RN_BUILD_DEBUG
		std::unordered_map<size_t, StrongRef<String>> _argumentSourceNames;
#endif
		RenderFrame *_activeRenderFrame;
		Lockable _frameLifecycleLock;
		Lockable _rendererAttachmentsLock;
		mutable Lockable _argumentSourceRegistryLock;
		mutable bool _hasResolvedArgumentSources;

		RenderingDevice *_device;
		RendererDescriptor *_descriptor;
		Dictionary *_defaultShaderCache[Shader::Type::COUNT][Shader::UsageHint::COUNT];
		Lockable _defaultShaderCacheLock;

		__RNDeclareMetaInternal(Renderer)
	};

	RNExceptionType(ShaderCompilation)
} // namespace RN


#endif /* __RAYNE_RENDERER_H_ */
