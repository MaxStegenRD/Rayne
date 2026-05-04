//
//  RNRenderer.cpp
//  Rayne
//
//  Copyright 2015 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNRenderer.h"
#include "RNRenderFrame.h"
#include "RNRenderPassResources.h"
#include "RNShadowRendererAttachment.h"
#include "../Base/RNKernel.h"
#include "../Debug/RNLogger.h"
#include "../Scene/RNLightClusterRendererAttachment.h"

namespace RN
{
	RNDefineMeta(RenderFramePresentationState, Object)
	RNDefineMeta(RenderPassDependencyProvider, Object)
	RNDefineMeta(RendererAttachment, Object)
	RNDefineMeta(Renderer, Object)

	RNExceptionImp(ShaderCompilation)

	static Renderer *_activeRenderer = nullptr;

	Renderer::Renderer(RendererDescriptor *descriptor, RenderingDevice *device) :
		_frameStatisticsTimer(0.0),
		_lastStartedRenderFrameID(0),
		_completedRenderFrameID(0),
		_lastRenderFrameDrawItemCount(0),
		_activeRenderFrame(nullptr),
		_hasResolvedShaderSources(false),
		_device(device),
		_descriptor(descriptor)
	{
		RN_ASSERT(descriptor, "Descriptor mustn't be NULL");
		RN_ASSERT(device, "Device mustn't be NULL");

		RegisterDefaultShaderSources();

		for(size_t typeIndex = 0; typeIndex < Shader::Type::COUNT; typeIndex++)
		{
			for(size_t hintIndex = 0; hintIndex < Shader::UsageHint::COUNT; hintIndex++)
			{
				_defaultShaderCache[typeIndex][hintIndex] = nullptr;
			}
		}
	}

	Renderer::~Renderer()
	{
		for(size_t typeIndex = 0; typeIndex < Shader::Type::COUNT; typeIndex++)
		{
			for(size_t hintIndex = 0; hintIndex < Shader::UsageHint::COUNT; hintIndex++)
			{
				SafeRelease(_defaultShaderCache[typeIndex][hintIndex]);
			}
		}
	}

	bool Renderer::IsHeadless()
	{
		return !_activeRenderer;
	}

	Renderer *Renderer::GetActiveRenderer()
	{
		RN_ASSERT(_activeRenderer, "GetActiveRenderer() called, but no renderer is currently active");
		return _activeRenderer;
	}

	RenderFramePresentationState::RenderFramePresentationState()
	{}

	RenderFramePresentationState::~RenderFramePresentationState()
	{}

	RenderPassDependencyProvider::RenderPassDependencyProvider()
	{}

	RenderPassDependencyProvider::~RenderPassDependencyProvider()
	{}

	void RenderPassDependencyProvider::CollectRenderPassDependencies(const RenderFrame::Pass &, RenderPassDependencyCollector &) const
	{}

	RendererAttachment::RendererAttachment()
	{}

	RendererAttachment::~RendererAttachment()
	{}

	void RendererAttachment::PrepareRenderFrame(Renderer *, RenderFrame &)
	{}

	bool RenderFramePresentationState::BeginFrameOnRenderThread()
	{
		return true;
	}

	void RenderFramePresentationState::EndFrameOnRenderThread()
	{}

	void RenderFramePresentationState::CancelFrameOnRenderThread()
	{}

	Matrix Renderer::GetProjectionCorrectionMatrix() const
	{
		return Matrix();
	}

	void Renderer::Activate()
	{
		RN_ASSERT(!_activeRenderer, "Rayne only supports one active renderer at a time");
		_activeRenderer = this;
	}

	void Renderer::Deactivate()
	{
		_activeRenderer = nullptr;
	}

	void Renderer::AddAttachment(RendererAttachment *attachment)
	{
		RN_ASSERT(attachment, "RendererAttachment mustn't be NULL");

		LockGuard<Lockable> lock(_rendererAttachmentsLock);
		for(const StrongRef<RendererAttachment> &existingAttachment : _rendererAttachments)
		{
			if(existingAttachment.Get() == attachment)
				return;
		}

		_rendererAttachments.push_back(attachment);
	}

	void Renderer::RemoveAttachment(RendererAttachment *attachment)
	{
		LockGuard<Lockable> lock(_rendererAttachmentsLock);
		_rendererAttachments.erase(std::remove_if(_rendererAttachments.begin(), _rendererAttachments.end(), [attachment](const StrongRef<RendererAttachment> &existingAttachment) {
			return existingAttachment.Get() == attachment;
		}), _rendererAttachments.end());
	}

	bool Renderer::HasAttachment(MetaClass *meta)
	{
		RN_ASSERT(meta, "RendererAttachment MetaClass mustn't be NULL");

		LockGuard<Lockable> lock(_rendererAttachmentsLock);
		for(const StrongRef<RendererAttachment> &attachment : _rendererAttachments)
		{
			if(attachment->IsKindOfClass(meta))
				return true;
		}

		return false;
	}

	void Renderer::SubmitAttachmentSnapshot(Object *snapshot)
	{
		RN_ASSERT(_activeRenderFrame, "SubmitAttachmentSnapshot() called outside render frame submission");
		_activeRenderFrame->AddAttachmentSnapshot(snapshot);
	}

	void Renderer::SubmitCameraPassAttachmentSnapshot(Object *snapshot)
	{
		RN_ASSERT(snapshot, "Camera pass attachment snapshot mustn't be NULL");
		RN_ASSERT(_activeRenderFrame, "SubmitCameraPassAttachmentSnapshot() called outside render frame submission");
		RN_ASSERT(!_cameraPassAttachmentSnapshotStack.empty(), "SubmitCameraPassAttachmentSnapshot() called outside camera submission");

		_cameraPassAttachmentSnapshotStack.back().push_back(snapshot);
	}

	void Renderer::RegisterShaderSource(const String *name, Shader::ArgumentBuffer::Source source)
	{
		RN_ASSERT(name, "Shader source name mustn't be NULL");
		RN_ASSERT(source == Shader::ArgumentBuffer::Source::Pass || source == Shader::ArgumentBuffer::Source::Frame, "Only pass and frame buffer shader sources can be registered");

		LockGuard<Lockable> lock(_shaderSourceRegistryLock);
		RN_ASSERT(!_hasResolvedShaderSources, "Shader sources must be registered before shader reflection");

		size_t nameHash = name->GetHash();
		auto iterator = _argumentBufferSources.find(nameHash);
		if(iterator != _argumentBufferSources.end())
		{
			RN_ASSERT(iterator->second == source, "Argument buffer source has already been registered with a different source");
			return;
		}

#if RN_BUILD_DEBUG
		TrackShaderSourceName(nameHash, name);
#endif
		_argumentBufferSources.emplace(nameHash, source);
	}

	void Renderer::RegisterShaderSource(const String *name, Shader::ArgumentTexture::Source source)
	{
		RN_ASSERT(name, "Shader source name mustn't be NULL");
		RN_ASSERT(source == Shader::ArgumentTexture::Source::Pass || source == Shader::ArgumentTexture::Source::Frame, "Only pass and frame texture shader sources can be registered");

		LockGuard<Lockable> lock(_shaderSourceRegistryLock);
		RN_ASSERT(!_hasResolvedShaderSources, "Shader sources must be registered before shader reflection");

		size_t nameHash = name->GetHash();
		auto iterator = _argumentTextureSources.find(nameHash);
		if(iterator != _argumentTextureSources.end())
		{
			RN_ASSERT(iterator->second == source, "Argument texture source has already been registered with a different source");
			return;
		}

#if RN_BUILD_DEBUG
		TrackShaderSourceName(nameHash, name);
#endif
		_argumentTextureSources.emplace(nameHash, source);
	}

	void Renderer::RegisterShaderSource(const String *name, Shader::UniformDescriptor::Source source)
	{
		RN_ASSERT(name, "Shader source name mustn't be NULL");
		RN_ASSERT(source == Shader::UniformDescriptor::Source::Pass, "Only pass uniform shader sources can be registered");

		LockGuard<Lockable> lock(_shaderSourceRegistryLock);
		RN_ASSERT(!_hasResolvedShaderSources, "Shader sources must be registered before shader reflection");

		size_t nameHash = name->GetHash();
		auto iterator = _uniformDescriptorSources.find(nameHash);
		if(iterator != _uniformDescriptorSources.end())
		{
			RN_ASSERT(iterator->second == source, "Uniform shader source has already been registered with a different source");
			return;
		}

#if RN_BUILD_DEBUG
		TrackShaderSourceName(nameHash, name);
#endif
		_uniformDescriptorSources.emplace(nameHash, source);
	}

	Shader::ArgumentBuffer::Source Renderer::GetShaderSource(const String *name, Shader::ArgumentBuffer::Source defaultSource) const
	{
		if(!name)
			return defaultSource;

		LockGuard<Lockable> lock(_shaderSourceRegistryLock);
		_hasResolvedShaderSources = true;

		size_t nameHash = name->GetHash();
		auto iterator = _argumentBufferSources.find(nameHash);
		if(iterator == _argumentBufferSources.end())
			return defaultSource;

#if RN_BUILD_DEBUG
		TrackShaderSourceName(nameHash, name);
#endif
		return iterator->second;
	}

	Shader::ArgumentTexture::Source Renderer::GetShaderSource(const String *name, Shader::ArgumentTexture::Source defaultSource) const
	{
		if(!name)
			return defaultSource;

		LockGuard<Lockable> lock(_shaderSourceRegistryLock);
		_hasResolvedShaderSources = true;

		size_t nameHash = name->GetHash();
		auto iterator = _argumentTextureSources.find(nameHash);
		if(iterator == _argumentTextureSources.end())
			return defaultSource;

#if RN_BUILD_DEBUG
		TrackShaderSourceName(nameHash, name);
#endif
		return iterator->second;
	}

	Shader::UniformDescriptor::Source Renderer::GetShaderSource(const String *name, Shader::UniformDescriptor::Source defaultSource) const
	{
		if(!name)
			return defaultSource;

		LockGuard<Lockable> lock(_shaderSourceRegistryLock);
		_hasResolvedShaderSources = true;

		size_t nameHash = name->GetHash();
		auto iterator = _uniformDescriptorSources.find(nameHash);
		if(iterator == _uniformDescriptorSources.end())
			return defaultSource;

#if RN_BUILD_DEBUG
		TrackShaderSourceName(nameHash, name);
#endif
		return iterator->second;
	}

#if RN_BUILD_DEBUG
	void Renderer::TrackShaderSourceName(size_t nameHash, const String *name) const
	{
		auto iterator = _shaderSourceNames.find(nameHash);
		RN_DEBUG_ASSERT(iterator == _shaderSourceNames.end() || iterator->second->IsEqual(name), "Shader source names have a hash collision");

		if(iterator == _shaderSourceNames.end())
			_shaderSourceNames[nameHash] = const_cast<String *>(name);
	}
#endif

	void Renderer::RegisterDefaultShaderSources()
	{
		ShadowRendererAttachment::RegisterShaderSources(this);
		LightClusterRendererAttachment::RegisterShaderSources(this);
	}

	RenderFrame *Renderer::SetActiveRenderFrame(RenderFrame *frame)
	{
		RenderFrame *previousFrame = _activeRenderFrame;
		_activeRenderFrame = frame;
		return previousFrame;
	}

	void Renderer::BeginCameraPassAttachmentSnapshots()
	{
		RN_ASSERT(_activeRenderFrame, "BeginCameraPassAttachmentSnapshots() called outside render frame submission");
		_cameraPassAttachmentSnapshotStack.emplace_back();
	}

	void Renderer::AddCameraPassAttachmentSnapshots(size_t passIndex)
	{
		RN_ASSERT(_activeRenderFrame, "AddCameraPassAttachmentSnapshots() called outside render frame submission");
		RN_ASSERT(!_cameraPassAttachmentSnapshotStack.empty(), "AddCameraPassAttachmentSnapshots() called outside camera submission");

		RenderFrame::Pass &pass = _activeRenderFrame->GetPass(passIndex);
		for(const StrongRef<Object> &snapshot : _cameraPassAttachmentSnapshotStack.back())
		{
			pass.AddAttachmentSnapshot(snapshot.Get());
		}
	}

	void Renderer::FinishCameraPassAttachmentSnapshots()
	{
		RN_ASSERT(!_cameraPassAttachmentSnapshotStack.empty(), "FinishCameraPassAttachmentSnapshots() called outside camera submission");
		_cameraPassAttachmentSnapshotStack.pop_back();
	}

	void Renderer::PrepareRendererAttachments(RenderFrame &frame)
	{
		std::vector<StrongRef<RendererAttachment>> attachments;
		{
			LockGuard<Lockable> lock(_rendererAttachmentsLock);
			attachments = _rendererAttachments;
		}

		for(const StrongRef<RendererAttachment> &attachment : attachments)
		{
			attachment->PrepareRenderFrame(this, frame);
		}
	}

	void Renderer::BeginRenderFrameSubmission(RenderFrame &frame)
	{
		size_t drawItemReserveCount;
		uint64 completedFrameID;
		{
			LockGuard<Lockable> lock(_frameLifecycleLock);
			_lastStartedRenderFrameID += 1;
			frame.SetFrameID(_lastStartedRenderFrameID);
			RN_PROFILE_ATRACE_ASYNC_BEGIN_N("RN RenderFrame", _lastStartedRenderFrameID);
			drawItemReserveCount = static_cast<size_t>(static_cast<float>(_lastRenderFrameDrawItemCount) * RN_RENDERING_DRAW_ITEM_RESERVE_MULTIPLIER);
			completedFrameID = _completedRenderFrameID;
		}

		DrainDrawableSnapshots(completedFrameID);
		frame.ReserveDrawItems(drawItemReserveCount);
	}

	void Renderer::FinishRenderFrameSubmission(const RenderFrame &frame)
	{
		uint64 frameID = frame.GetFrameID();
		{
			LockGuard<Lockable> lock(_frameLifecycleLock);
			if(frameID > _completedRenderFrameID)
				_completedRenderFrameID = frameID;
			_lastRenderFrameDrawItemCount = frame.GetDrawItemCount();
		}

		RN_PROFILE_ATRACE_ASYNC_END_N("RN RenderFrame", frameID);
		FlushDeletedDrawables();
	}

	void Renderer::QueueDrawableDeletion(Drawable *drawable)
	{
		UnregisterDrawableFromSnapshotDrain(drawable);

		LockGuard<Lockable> lock(_frameLifecycleLock);
		_pendingDeletedDrawables.push_back({ drawable, _lastStartedRenderFrameID });
	}

	void Renderer::RegisterDrawableForSnapshotDrain(Drawable *drawable)
	{
		LockGuard<Lockable> lock(_frameLifecycleLock);
		if(drawable->_isRegisteredForSnapshotDrain)
			return;

		drawable->_isRegisteredForSnapshotDrain = true;
		_drawablesPendingSnapshotDrain.push_back(drawable);
	}

	void Renderer::UnregisterDrawableFromSnapshotDrain(Drawable *drawable)
	{
		LockGuard<Lockable> lock(_frameLifecycleLock);
		if(!drawable->_isRegisteredForSnapshotDrain)
			return;

		drawable->_isRegisteredForSnapshotDrain = false;
		for(auto iterator = _drawablesPendingSnapshotDrain.begin(); iterator != _drawablesPendingSnapshotDrain.end(); ++iterator)
		{
			if(*iterator == drawable)
			{
				_drawablesPendingSnapshotDrain.erase(iterator);
				return;
			}
		}
	}

	void Renderer::DrainDrawableSnapshots(uint64 completedFrameID)
	{
		std::vector<Drawable *> drawables;
		{
			LockGuard<Lockable> lock(_frameLifecycleLock);
			drawables.swap(_drawablesPendingSnapshotDrain);
			for(Drawable *drawable : drawables)
				drawable->_isRegisteredForSnapshotDrain = false;
		}

		size_t pendingCount = 0;
		for(Drawable *drawable : drawables)
		{
			if(drawable->DrainDrawSnapshots(completedFrameID))
				drawables[pendingCount++] = drawable;
		}

		if(pendingCount == 0)
			return;

		LockGuard<Lockable> lock(_frameLifecycleLock);
		for(size_t i = 0; i < pendingCount; i += 1)
		{
			Drawable *drawable = drawables[i];
			if(drawable->_isRegisteredForSnapshotDrain)
				continue;

			drawable->_isRegisteredForSnapshotDrain = true;
			_drawablesPendingSnapshotDrain.push_back(drawable);
		}
	}

	void Renderer::FlushDeletedDrawables()
	{
		std::vector<Drawable *> drawables;
		{
			LockGuard<Lockable> lock(_frameLifecycleLock);
			size_t readyCount = 0;
			while(readyCount < _pendingDeletedDrawables.size() && _pendingDeletedDrawables[readyCount].frameID <= _completedRenderFrameID)
			{
				drawables.push_back(_pendingDeletedDrawables[readyCount].drawable);
				readyCount += 1;
			}

			_pendingDeletedDrawables.erase(_pendingDeletedDrawables.begin(), _pendingDeletedDrawables.begin() + readyCount);
		}

		for(Drawable *drawable : drawables)
			delete drawable;
	}

	void Renderer::FlushAllDeletedDrawables()
	{
		std::vector<Drawable *> drawables;
		{
			LockGuard<Lockable> lock(_frameLifecycleLock);
			for(const DeletedDrawable &deletedDrawable : _pendingDeletedDrawables)
				drawables.push_back(deletedDrawable.drawable);

			_pendingDeletedDrawables.clear();
		}

		for(Drawable *drawable : drawables)
			delete drawable;
	}

	void Renderer::PrintFrameStatistics(const RenderFrame &frame, float interval)
	{
		double currentTime = Kernel::GetSharedInstance()->GetTotalTime();
		if((currentTime - _frameStatisticsTimer) > interval)
		{
			_frameStatisticsTimer = currentTime;

			const std::vector<RenderFrame::CameraStatistics> &frameStatistics = frame.GetCameraStatistics();
			const size_t cameraCount = frameStatistics.size();
			uint64 totalVertices = 0;
			uint64 totalTriangles = 0;
			uint64 totalDrawables = 0;
			uint64 totalDrawCalls = 0;

			for(size_t i = 0; i < cameraCount; i++)
			{
				totalVertices += frameStatistics[i].numberOfVertices;
				totalTriangles += (frameStatistics[i].numberOfIndices / 3);
				totalDrawables += frameStatistics[i].numberOfDrawables;
				totalDrawCalls += frameStatistics[i].numberOfDrawCalls;
			}

			std::ostringstream statsStream;
			statsStream << "\nFrame stats | cameras=" << cameraCount
				<< " verts=" << totalVertices
				<< " tris=" << totalTriangles
				<< " drawables=" << totalDrawables
				<< " draws=" << totalDrawCalls;

			if(cameraCount > 1)
			{
				for(size_t i = 0; i < cameraCount; i++)
				{
					statsStream << "\n  cam " << i
						<< " | verts=" << frameStatistics[i].numberOfVertices
						<< " tris=" << (frameStatistics[i].numberOfIndices / 3)
						<< " drawables=" << frameStatistics[i].numberOfDrawables
						<< " draws=" << frameStatistics[i].numberOfDrawCalls;
				}
			}

			RNInfo(statsStream.str());
		}
	}

	void Renderer::ScheduleRenderThreadWork(Function &&function)
	{
		function();
	}

	void Renderer::SynchronizeRenderThread()
	{
	}

	void Renderer::WarmupDrawable(Mesh *mesh, Material *material, Camera *camera)
	{
	}

	RenderPassResources *Renderer::CreateRenderPassResources()
	{
		return new RenderPassResources(this);
	}

	void Renderer::DeleteRenderPassResources(RenderPassResources *resources)
	{
		delete resources;
	}

	Shader *Renderer::GetDefaultShader(Shader::Type type, Shader::Options *options, Shader::UsageHint hint)
	{
		Shader::Options *realOptions = options ? options->Copy() : Shader::Options::WithNone()->Retain();

		if(hint == Shader::UsageHint::Multiview || hint == Shader::UsageHint::DepthMultiview || hint == Shader::UsageHint::ShadowDepthMultiview)
		{
			realOptions->EnableMultiview();
		}

		const size_t typeIndex = static_cast<size_t>(type);
		const size_t hintIndex = static_cast<size_t>(hint);
		RN_ASSERT(hintIndex < Shader::UsageHint::COUNT, "Invalid shader usage hint");

		{
			LockGuard<Lockable> lock(_defaultShaderCacheLock);
			Dictionary *cache = _defaultShaderCache[typeIndex][hintIndex];
			if(cache)
			{
				Shader *cachedShader = cache->GetObjectForKey<Shader>(realOptions);
				if(cachedShader)
				{
					realOptions->Release();
					return cachedShader;
				}
			}
		}

		ShaderLibrary *shaderLibrary = GetDefaultShaderLibrary();
		Shader *shader = nullptr;
		if(type == Shader::Type::Vertex)
		{
			if(hint == Shader::UsageHint::Depth || hint == Shader::UsageHint::ShadowDepth)
			{
				shader = shaderLibrary->GetShaderWithName(RNCSTR("depth_vertex"), realOptions);
			}
			else
			{
				if(realOptions && realOptions->HasValue("RN_SKY", "1")) //Use a different shader for the sky
				{
					shader = shaderLibrary->GetShaderWithName(RNCSTR("sky_vertex"), realOptions);
				}
				else if(realOptions && realOptions->HasValue("RN_PARTICLES", "1"))
				{
					shader = shaderLibrary->GetShaderWithName(RNCSTR("particles_vertex"), realOptions);
				}
				else if(realOptions && realOptions->HasValue("RN_UI", "1"))
				{
					shader = shaderLibrary->GetShaderWithName(RNCSTR("ui_vertex"), realOptions);
				}
				else
				{
					shader = shaderLibrary->GetShaderWithName(RNCSTR("gouraud_vertex"), realOptions);
				}
			}
		}
		else if(type == Shader::Type::Fragment)
		{
			if(hint == Shader::UsageHint::Depth || hint == Shader::UsageHint::ShadowDepth)
			{
				shader = shaderLibrary->GetShaderWithName(RNCSTR("depth_fragment"), realOptions);
			}
			else
			{
				if(realOptions && realOptions->HasValue("RN_SKY", "1")) //Use a different shader for the sky
				{
					shader = shaderLibrary->GetShaderWithName(RNCSTR("sky_fragment"), realOptions);
				}
				else if(realOptions && realOptions->HasValue("RN_PARTICLES", "1"))
				{
					shader = shaderLibrary->GetShaderWithName(RNCSTR("particles_fragment"), realOptions);
				}
				else if(realOptions && realOptions->HasValue("RN_UI", "1"))
				{
					shader = shaderLibrary->GetShaderWithName(RNCSTR("ui_fragment"), realOptions);
				}
				else
				{
					shader = shaderLibrary->GetShaderWithName(RNCSTR("gouraud_fragment"), realOptions);
				}
			}
		}

		if(shader)
		{
			LockGuard<Lockable> lock(_defaultShaderCacheLock);
			Dictionary *cache = _defaultShaderCache[typeIndex][hintIndex];
			if(!cache)
			{
				cache = new Dictionary();
				_defaultShaderCache[typeIndex][hintIndex] = cache;
			}

			Shader *cachedShader = cache->GetObjectForKey<Shader>(realOptions);
			if(cachedShader)
			{
				shader = cachedShader;
			}
			else
			{
				cache->SetObjectForKey(shader, realOptions);
			}
		}

		realOptions->Release();

		return shader;
	}
} // namespace RN
