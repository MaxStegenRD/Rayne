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
#include "../Base/RNKernel.h"
#include "../Debug/RNLogger.h"

namespace RN
{
	RNDefineMeta(RenderFramePresentationState, Object)
	RNDefineMeta(Renderer, Object)

	RNExceptionImp(ShaderCompilation)

	static Renderer *_activeRenderer = nullptr;

	Renderer::Renderer(RendererDescriptor *descriptor, RenderingDevice *device) :
		_frameStatisticsTimer(0.0),
		_lastStartedRenderFrameID(0),
		_completedRenderFrameID(0),
		_lastRenderFrameDrawItemCount(0),
		_device(device),
		_descriptor(descriptor)
	{
		RN_ASSERT(descriptor, "Descriptor mustn't be NULL");
		RN_ASSERT(device, "Device mustn't be NULL");

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

	void RenderFramePresentationState::BeginFrameOnRenderThread()
	{}

	void RenderFramePresentationState::EndFrameOnRenderThread()
	{}

	void Renderer::Activate()
	{
		RN_ASSERT(!_activeRenderer, "Rayne only supports one active renderer at a time");
		_activeRenderer = this;
	}

	void Renderer::Deactivate()
	{
		_activeRenderer = nullptr;
	}

	void Renderer::BeginRenderFrameSubmission(RenderFrame &frame)
	{
		size_t drawItemReserveCount;
		uint64 completedFrameID;
		{
			LockGuard<Lockable> lock(_frameLifecycleLock);
			_lastStartedRenderFrameID += 1;
			frame.SetFrameID(_lastStartedRenderFrameID);
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
