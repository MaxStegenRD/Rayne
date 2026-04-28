//
//  RNRenderer.cpp
//  Rayne
//
//  Copyright 2015 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNRenderer.h"
#include "RNRenderPassResources.h"
#include "../Base/RNSettings.h"
#include "../Base/RNKernel.h"
#include "../Debug/RNLogger.h"

namespace RN
{
	RNDefineMeta(Renderer, Object)

	RNExceptionImp(ShaderCompilation)

	static Renderer *_activeRenderer = nullptr;

	Renderer::Renderer(RendererDescriptor *descriptor, RenderingDevice *device) :
		_device(device),
		_descriptor(descriptor),
		_updatePacketSlot(0),
		_renderPacketSlot(0)
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

	void Renderer::Activate()
	{
		RN_ASSERT(!_activeRenderer, "Rayne only supports one active renderer at a time");
		_activeRenderer = this;
	}

	void Renderer::Deactivate()
	{
		_activeRenderer = nullptr;
	}

	void Renderer::PrintFrameStatistics(float interval)
	{
		double currentTime = Kernel::GetSharedInstance()->GetTotalTime();
		if((currentTime - _frameStatisticsTimer) > 5.0)
		{
			_frameStatisticsTimer = currentTime;

			const size_t cameraCount = _frameStatistics.size();
			uint64 totalVertices = 0;
			uint64 totalTriangles = 0;
			uint64 totalDrawables = 0;
			uint64 totalDrawCalls = 0;

			for(size_t i = 0; i < cameraCount; i++)
			{
				totalVertices += _frameStatistics[i].numberOfVertices;
				totalTriangles += (_frameStatistics[i].numberOfIndices / 3);
				totalDrawables += _frameStatistics[i].numberOfDrawables;
				totalDrawCalls += _frameStatistics[i].numberOfDrawCalls;
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
						<< " | verts=" << _frameStatistics[i].numberOfVertices
						<< " tris=" << (_frameStatistics[i].numberOfIndices / 3)
						<< " drawables=" << _frameStatistics[i].numberOfDrawables
						<< " draws=" << _frameStatistics[i].numberOfDrawCalls;
				}
			}

			RNInfo(statsStream.str());
		}
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
