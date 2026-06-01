//
//  RNMetalShader.cpp
//  Rayne
//
//  Copyright 2015 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#import <Metal/Metal.h>
#include "RNMetalShader.h"
#include "RNMetalStateCoordinator.h"

namespace RN
{
	RNDefineMeta(MetalShader, Shader)

	MetalShader::MetalShader(ShaderLibrary *library, Type type, bool hasInstancing, const Array *samplers, const Shader::Options *options, const ComputeThreadsPerGroup &computeThreadsPerGroup, void *shader, MetalStateCoordinator *coordinator) :
		Shader(library, type, hasInstancing, options),
		_shader(shader),
		_coordinator(coordinator)
	{
		// We don't need to retain the shader because it was created
		// with [newFunctionWithName:] which returns an explicitly
		// owned object

		if(type == Shader::Type::Compute)
		{
			SetComputeThreadsPerGroup(computeThreadsPerGroup.x, computeThreadsPerGroup.y, computeThreadsPerGroup.z);
		}
		
		_rnSamplers = samplers->Retain();
		
		for(uint32 i = 0; i < static_cast<uint32>(Mesh::VertexAttribute::Feature::Custom) + 1; i++)
		{
			_hasInputVertexAttribute[i] = -1;
		}
		
		id<MTLFunction> shaderFunction = static_cast<id<MTLFunction>>(_shader);
		[shaderFunction.vertexAttributes enumerateObjectsUsingBlock:^(MTLVertexAttribute * _Nonnull attribute, NSUInteger idx, BOOL * _Nonnull stop) {
		 
			RN::String *name = RNSTR(attribute.name.UTF8String);
			uint32 location = attribute.attributeIndex;
		 
			if(name->IsEqual(RNCSTR("in_var_POSITION")))
			{
				_hasInputVertexAttribute[static_cast<uint32>(Mesh::VertexAttribute::Feature::Vertices)] = location;
			}
			else if (name->IsEqual(RNCSTR("in_var_NORMAL")))
			{
				_hasInputVertexAttribute[static_cast<uint32>(Mesh::VertexAttribute::Feature::Normals)] = location;
			}
			else if (name->IsEqual(RNCSTR("in_var_TANGENT")))
			{
				_hasInputVertexAttribute[static_cast<uint32>(Mesh::VertexAttribute::Feature::Tangents)] = location;
			}
			else if (name->IsEqual(RNCSTR("in_var_COLOR")) || name->IsEqual(RNCSTR("in_var_COLOR0")))
			{
				_hasInputVertexAttribute[static_cast<uint32>(Mesh::VertexAttribute::Feature::Color0)] = location;
			}
			else if (name->IsEqual(RNCSTR("in_var_COLOR1")))
			{
				_hasInputVertexAttribute[static_cast<uint32>(Mesh::VertexAttribute::Feature::Color1)] = location;
			}
			else if (name->IsEqual(RNCSTR("in_var_TEXCOORD")) || name->IsEqual(RNCSTR("in_var_TEXCOORD0")))
			{
				_hasInputVertexAttribute[static_cast<uint32>(Mesh::VertexAttribute::Feature::UVCoords0)] = location;
			}
			else if (name->IsEqual(RNCSTR("in_var_TEXCOORD1")))
			{
				_hasInputVertexAttribute[static_cast<uint32>(Mesh::VertexAttribute::Feature::UVCoords1)] = location;
			}
			else if (name->IsEqual(RNCSTR("in_var_BONEWEIGHTS")))
			{
				_hasInputVertexAttribute[static_cast<uint32>(Mesh::VertexAttribute::Feature::BoneWeights)] = location;
			}
			else if (name->IsEqual(RNCSTR("in_var_BONEINDICES")))
			{
				_hasInputVertexAttribute[static_cast<uint32>(Mesh::VertexAttribute::Feature::BoneIndices)] = location;
			}
			else if (name->IsEqual(RNCSTR("in_var_CUSTOM")))
			{
				_hasInputVertexAttribute[static_cast<uint32>(Mesh::VertexAttribute::Feature::Custom)] = location;
			}
		}];
	}

	MetalShader::~MetalShader()
	{
		id<MTLFunction> function = (id<MTLFunction>)_shader;
		[function release];
	}

	const String *MetalShader::GetName() const
	{
		id<MTLFunction> function = (id<MTLFunction>)_shader;
		NSString *name = [function name];

		return RNSTR([name UTF8String]);
	}

	void MetalShader::SetReflectedArguments(NSArray *arguments)
	{
		//TODO: Support custom uniformsdirectionalLights
		
		Array *buffersArray = new Array();
		Array *samplersArray = new Array();
		Array *texturesArray = new Array();
		Array *subpassInputsArray = new Array();

		for(MTLArgument *argument in arguments)
		{
			switch([argument type])
			{
				case MTLArgumentTypeBuffer:
				{
					//RNDebug("buffer: " << [[argument name] UTF8String]);
					String *argumentName = RNSTR([[argument name] UTF8String]);
					MTLStructType *structType = [argument bufferStructType];
					size_t numberOfElements = 0;
					Array *uniformDescriptors = GetBufferStructElements(structType, numberOfElements);
					
					if(uniformDescriptors->GetCount() > 0)
					{
						//numberOfElements will only be > 0 for per instance data. In this case marking it as storage buffer will make the renderer not limit the number of instances per draw call, as metal can handle this just fine (it's different with vulkan on some mobile hardware!)
						ArgumentBuffer *argumentBuffer = new ArgumentBuffer(argumentName, static_cast<uint32>([argument index]), uniformDescriptors, numberOfElements > 0? ArgumentBuffer::Type::StorageBuffer : ArgumentBuffer::Type::UniformBuffer, ArgumentBuffer::Source::Draw, numberOfElements > 0? 0 : 1);
						buffersArray->AddObject(argumentBuffer->Autorelease());
					}
					else
					{
						ArgumentBuffer::Source source = argumentName->HasPrefix(RNCSTR("vertexBuffer.")) ? ArgumentBuffer::Source::Draw : ArgumentBuffer::Source::Frame;
						// No reflected members: treat as raw/storage buffer (e.g., ByteAddressBuffer)
						ArgumentBuffer *argumentBuffer = new ArgumentBuffer(argumentName, static_cast<uint32>([argument index]), uniformDescriptors, ArgumentBuffer::Type::StorageBuffer, source, 0);
						buffersArray->AddObject(argumentBuffer->Autorelease());
					}
					
					break;
				}

				case MTLArgumentTypeTexture:
				{
					String *name = RNSTR([[argument name] UTF8String]);
					uint8 materialTextureIndex = 0;
					bool isSubpassInput = false;
					ArgumentTexture::Source textureSource = ArgumentTexture::Source::SubpassInput;
					ArgumentTexture::Type textureType = ([argument access] == MTLArgumentAccessReadOnly) ? ArgumentTexture::Type::Sampled : ArgumentTexture::Type::Storage;
					
					// Metal reports subpass inputs in the same texture argument list.
					if(name->HasPrefix(RNCSTR("colorInput")))
					{
						String *indexString = name->GetSubstring(Range(10, name->GetLength() - 10));
						materialTextureIndex = std::stoi(indexString->GetUTF8String());
						isSubpassInput = true;
					}
					else if(name->HasPrefix(RNCSTR("depthInput")))
					{
						String *indexString = name->GetSubstring(Range(10, name->GetLength() - 10));
						materialTextureIndex = std::stoi(indexString->GetUTF8String()) + 128;
						isSubpassInput = true;
					}
					else
					{
						textureSource = ArgumentTexture::GetSourceForName(name, materialTextureIndex);
					}
					
					ArgumentTexture *argumentTexture = new ArgumentTexture(name, static_cast<uint32>([argument index]), materialTextureIndex, textureSource, textureType);
					
					if(isSubpassInput) subpassInputsArray->AddObject(argumentTexture->Autorelease());
					else texturesArray->AddObject(argumentTexture->Autorelease());

					break;
				}

				case MTLArgumentTypeSampler:
				{
					String *name = RNSTR([[argument name] UTF8String]);
					ArgumentSampler *argumentSampler = ArgumentSampler::GetSamplerForName(name, static_cast<uint32>([argument index]), _rnSamplers);
					
					samplersArray->AddObject(argumentSampler->Autorelease());
					
					id<MTLSamplerState> samplerState = [_coordinator->GetSamplerStateForSampler(argumentSampler) retain];
					_samplers.push_back(samplerState);
					_samplerToIndexMapping.push_back([argument index]);

					break;
				}

				default:
					break;
			}
		}

		Signature *signature = new Signature(buffersArray->Autorelease(), samplersArray->Autorelease(), texturesArray->Autorelease(), subpassInputsArray->Autorelease());
		SetSignature(signature->Autorelease());
	}

	String *MetalShader::GetBufferStructMemberName(MTLStructMember *member) const
	{
		String *name = RNSTR([[member name] UTF8String]);
		if(name->HasSuffix(RNCSTR("[0]")))
		{
			return name->GetSubstring(Range(0, name->GetLength() - 3));
		}

		return name;
	}

	PrimitiveType MetalShader::GetPrimitiveTypeForMetalDataType(MTLDataType type) const
	{
		switch(type)
		{
			case MTLDataTypeHalf:
				return PrimitiveType::Half;

			case MTLDataTypeHalf2:
				return PrimitiveType::HalfVector2;

			case MTLDataTypeHalf3:
				return PrimitiveType::HalfVector3;

			case MTLDataTypeHalf4:
				return PrimitiveType::HalfVector4;

			case MTLDataTypeFloat:
				return PrimitiveType::Float;

			case MTLDataTypeFloat2:
				return PrimitiveType::Vector2;

			case MTLDataTypeFloat3:
				return PrimitiveType::Vector3;

			case MTLDataTypeFloat4:
				return PrimitiveType::Vector4;

			case MTLDataTypeFloat2x2:
				return PrimitiveType::Matrix2x2;

			case MTLDataTypeFloat3x3:
				return PrimitiveType::Matrix3x3;

			case MTLDataTypeFloat4x4:
				return PrimitiveType::Matrix4x4;

			case MTLDataTypeInt:
				return PrimitiveType::Int32;

			case MTLDataTypeUInt:
				return PrimitiveType::Uint32;

			case MTLDataTypeShort:
				return PrimitiveType::Int16;

			case MTLDataTypeUShort:
				return PrimitiveType::Uint16;

			case MTLDataTypeChar:
				return PrimitiveType::Int8;

			case MTLDataTypeUChar:
				return PrimitiveType::Uint8;

			default:
				return PrimitiveType::Invalid;
		}
	}

	Array *MetalShader::GetBufferStructElements(MTLStructType *structType, size_t &numberOfElements)
	{
		Array *uniformDescriptors = new Array();
		uniformDescriptors->Autorelease();
		
		numberOfElements = 0;
		
		NSArray<MTLStructMember *> *members = [structType members];
		for(MTLStructMember *member in members)
		{
			String *name = GetBufferStructMemberName(member);
			uint32 offset = [member offset];
			MTLDataType type = [member dataType];
			
			uint32 arrayElementCount = 1;
			
			//RNDebug("	buffer member: " << name << " type: " << type);
			if(type == MTLDataTypeArray)
			{
				MTLArrayType *arrayType = [member arrayType];
				arrayElementCount = arrayType.arrayLength;
				MTLDataType elementType = arrayType.elementType;

				// ShaderConductor wraps raw buffers as a single primitive _m0 array. Keep those as storage buffers.
				if(members.count == 1 && name->IsEqual(RNCSTR("_m0")) && elementType != MTLDataTypeStruct)
				{
					continue;
				}

				// If the whole buffer is an array of structs with unknown name, assume that it is per instance data.
				if(members.count == 1 && elementType == MTLDataTypeStruct && !UniformDescriptor::IsKnownStructName(name))
				{
					MTLStructType *otherStructType = [arrayType elementStructType];
					if(otherStructType)
					{
						numberOfElements = arrayElementCount;
						size_t temp = 0;
						return GetBufferStructElements(otherStructType, temp);
					}
				}

				type = elementType;
			}

			PrimitiveType uniformType = GetPrimitiveTypeForMetalDataType(type);
			Shader::UniformDescriptor *descriptor = new Shader::UniformDescriptor(name, uniformType, offset, arrayElementCount);
			uniformDescriptors->AddObject(descriptor->Autorelease());
		}
		
		return uniformDescriptors;
	}
}
