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
#include "RNRendererTypes.h"
#include "RNShaderLibrary.h"
#include "RNSkeleton.h"
#include "RNTexture.h"
#include "RNWindow.h"

namespace RN
{
	struct RenderPassResources;

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

		uint8 GetUpdatePacketSlot() const { return _updatePacketSlot; }
		uint8 GetRenderPacketSlot() const { return _renderPacketSlot; }

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
		uint8 _updatePacketSlot;
		uint8 _renderPacketSlot;
		Dictionary *_defaultShaderCache[Shader::Type::COUNT][Shader::UsageHint::COUNT];
		Lockable _defaultShaderCacheLock;

		__RNDeclareMetaInternal(Renderer)
	};

	RNExceptionType(ShaderCompilation)
} // namespace RN


#endif /* __RAYNE_RENDERER_H_ */
