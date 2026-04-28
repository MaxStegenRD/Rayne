//
//  RNMetalStateCoordinator.cpp
//  Rayne
//
//  Copyright 2015 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNMetalStateCoordinator.h"
#include "RNMetalShader.h"
#include "RNMetalFramebuffer.h"
#include "RNMetalTexture.h"

namespace RN
{
	MTLVertexFormat _vertexFormatLookup[] =
	{
		MTLVertexFormatInvalid,
		
		MTLVertexFormatUChar2,
		MTLVertexFormatUShort2,
		MTLVertexFormatUInt,

		MTLVertexFormatChar2,
		MTLVertexFormatShort2,
		MTLVertexFormatInt,

		MTLVertexFormatHalf,
		MTLVertexFormatHalf2,
		MTLVertexFormatHalf3,
		MTLVertexFormatHalf4,

		MTLVertexFormatFloat,
		MTLVertexFormatFloat2,
		MTLVertexFormatFloat3,
		MTLVertexFormatFloat4,
		
		MTLVertexFormatFloat2,
		MTLVertexFormatFloat3,
		MTLVertexFormatFloat4,
		
		MTLVertexFormatFloat4,
		MTLVertexFormatFloat4
	};

	MTLCompareFunction CompareFunctionLookup[] =
	{
		MTLCompareFunctionNever,
		MTLCompareFunctionAlways,
		MTLCompareFunctionLess,
		MTLCompareFunctionLessEqual,
		MTLCompareFunctionEqual,
		MTLCompareFunctionNotEqual,
		MTLCompareFunctionGreaterEqual,
		MTLCompareFunctionGreater
	};

	MetalStateCoordinator::MetalStateCoordinator() :
		_device(nullptr),
		_lastDepthStencilState(nullptr)
	{}

	MetalStateCoordinator::~MetalStateCoordinator()
	{
		for(MetalRenderingStateCollection *collection : _renderingStates)
			delete collection;

		for(MetalDepthStencilState *state : _depthStencilStates)
			delete state;

		for(auto &pair : _samplers)
		{
			[pair.first release];
			pair.second->Release();
		}
	}

	void MetalStateCoordinator::SetDevice(id<MTLDevice> device)
	{
		_device = device;
	}


	id<MTLDepthStencilState> MetalStateCoordinator::GetDepthStencilStateForMaterial(const Material::PipelineProperties &materialProperties, const MetalRenderingState *renderingState)
	{
		if(RN_EXPECT_TRUE(_lastDepthStencilState != nullptr) && _lastDepthStencilState->MatchesMaterial(materialProperties, renderingState->depthFormat, renderingState->stencilFormat))
			return _lastDepthStencilState->depthStencilState;

		for(const MetalDepthStencilState *state : _depthStencilStates)
		{
			if(state->MatchesMaterial(materialProperties, renderingState->depthFormat, renderingState->stencilFormat))
			{
				_lastDepthStencilState = state;
				return _lastDepthStencilState->depthStencilState;
			}
		}

		MTLDepthStencilDescriptor *descriptor = [[MTLDepthStencilDescriptor alloc] init];
		
		if(renderingState->depthFormat != MTLPixelFormatInvalid)
		{
			[descriptor setDepthWriteEnabled:materialProperties.depthWriteEnabled];
			[descriptor setDepthCompareFunction:CompareFunctionLookup[static_cast<uint32_t>(materialProperties.depthMode)]];
		}
		else
		{
			[descriptor setDepthWriteEnabled:NO];
			[descriptor setDepthCompareFunction:CompareFunctionLookup[1]];
		}

		id<MTLDepthStencilState> state = [_device newDepthStencilStateWithDescriptor:descriptor];
		_lastDepthStencilState = new MetalDepthStencilState(materialProperties, state, renderingState->depthFormat, renderingState->stencilFormat);

		_depthStencilStates.push_back(const_cast<MetalDepthStencilState *>(_lastDepthStencilState));
		[descriptor release];

		return _lastDepthStencilState->depthStencilState;
	}

	id<MTLSamplerState> MetalStateCoordinator::GetSamplerStateForSampler(const Shader::ArgumentSampler *samplerDescriptor)
	{
		std::lock_guard<std::mutex> lock(_samplerLock);

		for(auto &pair : _samplers)
		{
			if(pair.second == samplerDescriptor)
				return pair.first;
		}


		MTLSamplerDescriptor *descriptor = [[MTLSamplerDescriptor alloc] init];

		switch(samplerDescriptor->GetWrapMode())
		{
			case Shader::ArgumentSampler::WrapMode::Clamp:
				[descriptor setRAddressMode:MTLSamplerAddressModeClampToEdge];
				[descriptor setSAddressMode:MTLSamplerAddressModeClampToEdge];
				[descriptor setTAddressMode:MTLSamplerAddressModeClampToEdge];
				break;
			case Shader::ArgumentSampler::WrapMode::Repeat:
				[descriptor setRAddressMode:MTLSamplerAddressModeRepeat];
				[descriptor setSAddressMode:MTLSamplerAddressModeRepeat];
				[descriptor setTAddressMode:MTLSamplerAddressModeRepeat];
				break;
		}

		MTLSamplerMipFilter mipFilter;
		switch(samplerDescriptor->GetFilter())
		{
			case Shader::ArgumentSampler::Filter::Anisotropic:
			{
				NSUInteger anisotropy = std::min(static_cast<uint8>(16), std::max(static_cast<uint8>(1), samplerDescriptor->GetAnisotropy()));
				[descriptor setMaxAnisotropy:anisotropy];
			}

			case Shader::ArgumentSampler::Filter::Linear:
				[descriptor setMinFilter:MTLSamplerMinMagFilterLinear];
				[descriptor setMagFilter:MTLSamplerMinMagFilterLinear];

				mipFilter = MTLSamplerMipFilterLinear;
				break;

			case Shader::ArgumentSampler::Filter::Nearest:
				[descriptor setMinFilter:MTLSamplerMinMagFilterNearest];
				[descriptor setMagFilter:MTLSamplerMinMagFilterNearest];

				mipFilter = MTLSamplerMipFilterNearest;
				break;
		}
		[descriptor setMipFilter:mipFilter];
		
		switch(samplerDescriptor->GetComparisonFunction())
		{
			case Shader::ArgumentSampler::ComparisonFunction::Never:
				[descriptor setCompareFunction:MTLCompareFunctionNever];
				break;
				
			case Shader::ArgumentSampler::ComparisonFunction::Less:
				[descriptor setCompareFunction:MTLCompareFunctionLess];
				break;
				
			case Shader::ArgumentSampler::ComparisonFunction::LessEqual:
				[descriptor setCompareFunction:MTLCompareFunctionLessEqual];
				break;
				
			case Shader::ArgumentSampler::ComparisonFunction::Equal:
				[descriptor setCompareFunction:MTLCompareFunctionEqual];
				break;
				
			case Shader::ArgumentSampler::ComparisonFunction::NotEqual:
				[descriptor setCompareFunction:MTLCompareFunctionNotEqual];
				break;
				
			case Shader::ArgumentSampler::ComparisonFunction::GreaterEqual:
				[descriptor setCompareFunction:MTLCompareFunctionGreaterEqual];
				break;
				
			case Shader::ArgumentSampler::ComparisonFunction::Greater:
				[descriptor setCompareFunction:MTLCompareFunctionGreater];
				break;
				
			case Shader::ArgumentSampler::ComparisonFunction::Always:
				[descriptor setCompareFunction:MTLCompareFunctionAlways];
				break;
		}

		id<MTLSamplerState> sampler = [_device newSamplerStateWithDescriptor:descriptor];
		[descriptor release];

		_samplers.emplace_back(std::make_pair(sampler, samplerDescriptor->Retain()));

		return sampler;
	}

	const MetalRenderingState *MetalStateCoordinator::GetRenderPipelineState(Shader *vertexShader, Shader *fragmentShader, Mesh *mesh, Framebuffer *framebuffer, const Material::PipelineProperties &materialProperties, const RenderPass::DrawSnapshot &drawSnapshot)
	{
		const Mesh::VertexDescriptor &descriptor = mesh->GetVertexDescriptor();

		MetalShader *metalVertexShader = static_cast<MetalShader *>(vertexShader);
		MetalShader *metalFragmentShader = static_cast<MetalShader *>(fragmentShader);

		for(MetalRenderingStateCollection *collection : _renderingStates)
		{
			if(collection->descriptor.IsEqual(descriptor))
			{
				if(collection->fragmentShader->IsEqual(metalFragmentShader) && collection->vertexShader->IsEqual(metalVertexShader))
				{
					return GetRenderPipelineStateInCollection(collection, mesh, framebuffer, materialProperties, drawSnapshot);
				}
			}
		}

		MetalRenderingStateCollection *collection = new MetalRenderingStateCollection(descriptor, metalVertexShader, metalFragmentShader);
		_renderingStates.push_back(collection);

		return GetRenderPipelineStateInCollection(collection, mesh, framebuffer, materialProperties, drawSnapshot);
	}

	const MetalRenderingState *MetalStateCoordinator::GetRenderPipelineStateInCollection(MetalRenderingStateCollection *collection, Mesh *mesh, Framebuffer *framebuffer, const Material::PipelineProperties &materialProperties, const RenderPass::DrawSnapshot &drawSnapshot)
	{
		MetalFramebuffer *metalFramebuffer = framebuffer->Downcast<MetalFramebuffer>();
		const RenderPass::SubpassSnapshot &subpass = drawSnapshot.GetSubpass();
		std::vector<MTLPixelFormat> pixelFormats;
		for(uint32 i = 0; i < metalFramebuffer->GetColorTargetCount(); i++)
		{
			if(drawSnapshot.IsSubpass() && !subpass.GetColorAttachment(i).GetUses())
			{
				continue;
			}

			pixelFormats.push_back(metalFramebuffer->GetMetalColorFormat(i));
		}
		MTLPixelFormat depthFormat = metalFramebuffer->GetMetalDepthFormat();
		MTLPixelFormat stencilFormat = metalFramebuffer->GetMetalStencilFormat();
		uint8 sampleCount = metalFramebuffer->GetSampleCount();
		
		for(const MetalRenderingState *state : collection->states)
		{
			if(state->pixelFormats == pixelFormats && state->depthFormat == depthFormat && state->stencilFormat == stencilFormat && state->sampleCount == sampleCount && state->wantsAlphaToCoverage == materialProperties.useAlphaToCoverage && state->colorWriteMask == materialProperties.colorWriteMask && state->blendOperationRGB == materialProperties.blendOperationRGB && state->blendOperationAlpha == materialProperties.blendOperationAlpha && state->blendFactorSourceRGB == materialProperties.blendFactorSourceRGB && state->blendFactorSourceAlpha == materialProperties.blendFactorSourceAlpha && state->blendFactorDestinationRGB == materialProperties.blendFactorDestinationRGB && state->blendFactorDestinationAlpha == materialProperties.blendFactorDestinationAlpha)
				return state;
		}

		MTLVertexDescriptor *vertexDescriptor = CreateVertexDescriptorFromMesh(mesh, static_cast<MetalShader*>(collection->vertexShader));

		MTLRenderPipelineDescriptor *pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
		pipelineStateDescriptor.vertexFunction = static_cast<id>(collection->vertexShader->_shader);
		pipelineStateDescriptor.fragmentFunction = static_cast<id>(collection->fragmentShader->_shader);
		pipelineStateDescriptor.vertexDescriptor = vertexDescriptor;
		pipelineStateDescriptor.sampleCount = sampleCount;

		switch(mesh->GetDrawMode())
		{
			case DrawMode::Point:
				pipelineStateDescriptor.inputPrimitiveTopology = MTLPrimitiveTopologyClassPoint;
				break;
			case DrawMode::Line:
			case DrawMode::LineStrip:
				pipelineStateDescriptor.inputPrimitiveTopology = MTLPrimitiveTopologyClassLine;
				break;
			case DrawMode::Triangle:
			case DrawMode::TriangleStrip:
				pipelineStateDescriptor.inputPrimitiveTopology = MTLPrimitiveTopologyClassTriangle;
				break;
		}

		int attachmentCounter = 0;
		pipelineStateDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatInvalid;
		for(uint32 targetCounter = 0; targetCounter < metalFramebuffer->GetColorTargetCount(); targetCounter++)
		{
			if(drawSnapshot.IsSubpass() && !subpass.GetColorAttachment(targetCounter).GetUses())
			{
				continue;
			}

			MTLPixelFormat pixelFormat = metalFramebuffer->GetMetalColorFormat(targetCounter);
			pipelineStateDescriptor.colorAttachments[attachmentCounter].pixelFormat = pixelFormat;
			pipelineStateDescriptor.colorAttachments[attachmentCounter].writeMask = 0;
			if(materialProperties.colorWriteMask & (1 << 0)) pipelineStateDescriptor.colorAttachments[attachmentCounter].writeMask |= MTLColorWriteMaskRed;
			if(materialProperties.colorWriteMask & (1 << 1)) pipelineStateDescriptor.colorAttachments[attachmentCounter].writeMask |= MTLColorWriteMaskGreen;
			if(materialProperties.colorWriteMask & (1 << 2)) pipelineStateDescriptor.colorAttachments[attachmentCounter].writeMask |= MTLColorWriteMaskBlue;
			if(materialProperties.colorWriteMask & (1 << 3)) pipelineStateDescriptor.colorAttachments[attachmentCounter].writeMask |= MTLColorWriteMaskAlpha;
			pipelineStateDescriptor.colorAttachments[attachmentCounter].blendingEnabled = false;
			if(pixelFormat != MTLPixelFormatInvalid && materialProperties.blendOperationRGB != BlendOperation::None && materialProperties.blendOperationAlpha != BlendOperation::None)
			{
				pipelineStateDescriptor.colorAttachments[attachmentCounter].blendingEnabled = true;
				pipelineStateDescriptor.colorAttachments[attachmentCounter].rgbBlendOperation = static_cast<MTLBlendOperation>(materialProperties.blendOperationRGB);
				pipelineStateDescriptor.colorAttachments[attachmentCounter].sourceRGBBlendFactor = static_cast<MTLBlendFactor>(materialProperties.blendFactorSourceRGB);
				pipelineStateDescriptor.colorAttachments[attachmentCounter].destinationRGBBlendFactor = static_cast<MTLBlendFactor>(materialProperties.blendFactorDestinationRGB);
				pipelineStateDescriptor.colorAttachments[attachmentCounter].alphaBlendOperation = static_cast<MTLBlendOperation>(materialProperties.blendOperationAlpha);
				pipelineStateDescriptor.colorAttachments[attachmentCounter].sourceAlphaBlendFactor = static_cast<MTLBlendFactor>(materialProperties.blendFactorSourceAlpha);
				pipelineStateDescriptor.colorAttachments[attachmentCounter].destinationAlphaBlendFactor = static_cast<MTLBlendFactor>(materialProperties.blendFactorDestinationAlpha);
			}
			attachmentCounter += 1;
		}

		pipelineStateDescriptor.depthAttachmentPixelFormat = depthFormat;
		pipelineStateDescriptor.stencilAttachmentPixelFormat = stencilFormat;
		pipelineStateDescriptor.alphaToCoverageEnabled = materialProperties.useAlphaToCoverage;

		id<MTLRenderPipelineState> pipelineState = nil;
		if(collection->vertexShader->GetSignature() && collection->fragmentShader->GetSignature())
		{
			NSError *error = nil;
			pipelineState = [_device newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&error];
			RN_ASSERT(!error, "PipelineState creation failed with error: %s", error.localizedDescription.UTF8String);
		}
		else
		{
			MTLRenderPipelineReflection * reflection;
			
			NSError *error = nil;
			pipelineState = [_device newRenderPipelineStateWithDescriptor:pipelineStateDescriptor options:MTLPipelineOptionBufferTypeInfo reflection:&reflection error:&error];

			RN_ASSERT(!error, "PipelineState creation failed with error: %s", error.localizedDescription.UTF8String);

			if(!collection->vertexShader->GetSignature())
			{
				collection->vertexShader->SetReflectedArguments([reflection vertexArguments]);
			}

			if(!collection->fragmentShader->GetSignature())
			{
				collection->fragmentShader->SetReflectedArguments([reflection fragmentArguments]);
			}
		}

		[pipelineStateDescriptor release];
		[vertexDescriptor release];

		// Create the rendering state
		MetalRenderingState *state = new MetalRenderingState();
		state->state = pipelineState;
		state->pixelFormats = pixelFormats;
		state->depthFormat = depthFormat;
		state->stencilFormat = stencilFormat;
		state->sampleCount = sampleCount;
		state->vertexShader = collection->vertexShader;
		state->fragmentShader = collection->fragmentShader;
		state->vertexPositionBufferShaderResourceIndex = mesh->GetVertexPositionsSeparatedSize() > 0? 29 : 255; //Hardcoded to match value CreateVertexDescriptorFromMesh
		state->vertexBufferShaderResourceIndex = 30; //Hardcoded to match value CreateVertexDescriptorFromMesh
		state->wantsAlphaToCoverage = materialProperties.useAlphaToCoverage;
		state->colorWriteMask = materialProperties.colorWriteMask;
		state->blendOperationRGB = materialProperties.blendOperationRGB;
		state->blendOperationAlpha = materialProperties.blendOperationAlpha;
		state->blendFactorSourceRGB = materialProperties.blendFactorSourceRGB;
		state->blendFactorDestinationRGB = materialProperties.blendFactorDestinationRGB;
		state->blendFactorSourceAlpha = materialProperties.blendFactorSourceAlpha;
		state->blendFactorDestinationAlpha = materialProperties.blendFactorDestinationAlpha;

		collection->states.push_back(state);

		return state;
	}

	MTLVertexDescriptor *MetalStateCoordinator::CreateVertexDescriptorFromMesh(Mesh *mesh, MetalShader *shader)
	{
		MTLVertexDescriptor *descriptor = [[MTLVertexDescriptor alloc] init];
		
		//Hardcoding the binding index to 29 for positions and 30 for everything else here as there is no way to figure out the ones in use before reflection, but need to create the pipeline for reflection to be available... 30 is the highest index allowed!
		
		if(mesh->GetVertexPositionsSeparatedSize() > 0)
		{
			bool didSetBufferAttributes = false;
			
			const std::vector<Mesh::VertexAttribute> &attributes = mesh->GetVertexAttributes();
			for(const Mesh::VertexAttribute &attribute : attributes)
			{
				if(attribute.GetFeature() != Mesh::VertexAttribute::Feature::Vertices) continue;

				uint32 attributeIndex = shader->_hasInputVertexAttribute[static_cast<int>(attribute.GetFeature())];
				if(attributeIndex == -1) continue;
				
				MTLVertexAttributeDescriptor *attributeDescriptor = descriptor.attributes[attributeIndex];
				attributeDescriptor.format = _vertexFormatLookup[static_cast<MTLVertexFormat>(attribute.GetType())];
				attributeDescriptor.offset = attribute.GetOffset();
				attributeDescriptor.bufferIndex = 29;
				
				didSetBufferAttributes = true;
				
				break;
			}
			
			if(didSetBufferAttributes)
			{
				descriptor.layouts[29].stride = mesh->GetVertexPositionsSeparatedStride();
				descriptor.layouts[29].stepFunction = MTLVertexStepFunctionPerVertex;
				descriptor.layouts[29].stepRate = 1;
			}
		}

		bool didSetBufferAttributes = false;
		const std::vector<Mesh::VertexAttribute> &attributes = mesh->GetVertexAttributes();
		for(const Mesh::VertexAttribute &attribute : attributes)
		{
			if(attribute.GetFeature() == Mesh::VertexAttribute::Feature::Indices) continue;
			if(mesh->GetVertexPositionsSeparatedSize() > 0 && attribute.GetFeature() == Mesh::VertexAttribute::Feature::Vertices) continue;

			uint32 attributeIndex = shader->_hasInputVertexAttribute[static_cast<int>(attribute.GetFeature())];
			if(attributeIndex == -1) continue;
			
			MTLVertexAttributeDescriptor *attributeDescriptor = descriptor.attributes[attributeIndex];
			attributeDescriptor.format = _vertexFormatLookup[static_cast<MTLVertexFormat>(attribute.GetType())];
			attributeDescriptor.offset = attribute.GetOffset();
			attributeDescriptor.bufferIndex = 30;
			
			didSetBufferAttributes = true;
		}
		
		if(didSetBufferAttributes)
		{
			descriptor.layouts[30].stride = mesh->GetStride();
			descriptor.layouts[30].stepFunction = MTLVertexStepFunctionPerVertex;
			descriptor.layouts[30].stepRate = 1;
		}

		return descriptor;
	}
}
