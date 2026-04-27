//
//  RNMaterial.cpp
//  Rayne
//
//  Copyright 2015 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNMaterial.h"
#include "RNFramebuffer.h"

namespace RN
{
	RNDefineMeta(Material, Object)

	Material::Properties::Properties() :
		ambientColor(Color(0.5f, 0.5f, 0.5f, 1.0f)), diffuseColor(Color(1.0f, 1.0f, 1.0f, 1.0f)), specularColor(Color(1.0f, 1.0f, 1.0f, 4.0f)), emissiveColor(Color(0.0f, 0.0f, 0.0f, 0.0f)), alphaToCoverageClamp(1.0f), textureTileFactor(1.0f)
	{}

	Material::Properties::Properties(const Properties &properties)
	{
		CopyFromProperties(properties);
	}

	Material::Properties::~Properties()
	{
		ClearCustomShaderUniforms();
	}

	void Material::Properties::ClearCustomShaderUniforms()
	{
		for(auto const &data : _customShaderUniforms)
		{
			data.second->Release();
		}
		_customShaderUniforms.clear();
	}

	void Material::Properties::CopyFromProperties(const Properties &properties)
	{
		if(this == &properties) return;

		ambientColor = properties.ambientColor;
		diffuseColor = properties.diffuseColor;
		specularColor = properties.specularColor;
		emissiveColor = properties.emissiveColor;
		alphaToCoverageClamp = properties.alphaToCoverageClamp;
		textureTileFactor = properties.textureTileFactor;

		customMatrix1 = properties.customMatrix1;
		customMatrix2 = properties.customMatrix2;

		uiClippingRect = properties.uiClippingRect;
		uiOffset = properties.uiOffset;
		uiOutlineColor = properties.uiOutlineColor;

		ClearCustomShaderUniforms();
		_customShaderUniforms.insert(properties._customShaderUniforms.begin(), properties._customShaderUniforms.end());

		for(auto const &data : _customShaderUniforms)
		{
			data.second->Retain();
		}
	}

	void Material::Properties::SetCustomShaderUniform(const String *name, Value *value)
	{
		const size_t nameHash = name->GetHash();
		Object *oldValue = _customShaderUniforms[nameHash];
		_customShaderUniforms[nameHash] = value->Retain();
		SafeRelease(oldValue);
	}

	void Material::Properties::SetCustomShaderUniform(const String *name, Number *number)
	{
		const size_t nameHash = name->GetHash();
		Object *oldValue = _customShaderUniforms[nameHash];
		_customShaderUniforms[nameHash] = number->Retain();
		SafeRelease(oldValue);
	}

	Object *Material::Properties::GetCustomShaderUniform(const String *name) const
	{
		const auto result = _customShaderUniforms.find(name->GetHash());
		if(result != _customShaderUniforms.end())
		{
			return result->second;
		}

		return nullptr;
	}

	Object *Material::Properties::GetCustomShaderUniform(size_t nameHash) const
	{
		const auto result = _customShaderUniforms.find(nameHash);
		if(result != _customShaderUniforms.end())
		{
			return result->second;
		}

		return nullptr;
	}

	Material::PipelineProperties::PipelineProperties() :
		colorWriteMask(0xf), depthMode(DepthMode::Greater), depthWriteEnabled(true), usePolygonOffset(false), polygonOffsetFactor(-1.1f), polygonOffsetUnits(-0.1f), useAlphaToCoverage(false), cullMode(CullMode::BackFace), blendOperationRGB(BlendOperation::None), blendOperationAlpha(BlendOperation::None), blendFactorSourceRGB(BlendFactor::SourceAlpha), blendFactorDestinationRGB(BlendFactor::OneMinusSourceAlpha), blendFactorSourceAlpha(BlendFactor::One), blendFactorDestinationAlpha(BlendFactor::One)
	{}

	Material::PipelineProperties::PipelineProperties(const PipelineProperties &properties)
	{
		CopyFromPipelineProperties(properties);
	}

	Material::PipelineProperties::~PipelineProperties()
	{

	}

	void Material::PipelineProperties::CopyFromPipelineProperties(const PipelineProperties &properties)
	{
		if(this == &properties) return;

		colorWriteMask = properties.colorWriteMask;
		depthMode = properties.depthMode;
		depthWriteEnabled = properties.depthWriteEnabled;
		usePolygonOffset = properties.usePolygonOffset;
		polygonOffsetFactor = properties.polygonOffsetFactor;
		polygonOffsetUnits = properties.polygonOffsetUnits;
		useAlphaToCoverage = properties.useAlphaToCoverage;
		cullMode = properties.cullMode;
		blendOperationRGB = properties.blendOperationRGB;
		blendOperationAlpha = properties.blendOperationAlpha;
		blendFactorSourceRGB = properties.blendFactorSourceRGB;
		blendFactorDestinationRGB = properties.blendFactorDestinationRGB;
		blendFactorSourceAlpha = properties.blendFactorSourceAlpha;
		blendFactorDestinationAlpha = properties.blendFactorDestinationAlpha;
	}

	void Material::DrawSnapshot::Reset()
	{
		_textures = nullptr;

		for(uint8 i = 0; i < static_cast<uint8>(Shader::UsageHint::COUNT); i++)
		{
			_vertexShader[i] = nullptr;
			_fragmentShader[i] = nullptr;
		}

		_override = 0;

		Properties defaultProperties;
		_properties.CopyFromProperties(defaultProperties);

		PipelineProperties defaultPipelineProperties;
		_pipelineProperties.CopyFromPipelineProperties(defaultPipelineProperties);
	}

	Shader *Material::DrawSnapshot::GetFragmentShader(Shader::UsageHint type) const
	{
		uint8 index = static_cast<uint8>(type);
		if(!_fragmentShader[index].Get() && !_vertexShader[index].Get())
			return _fragmentShader[static_cast<uint8>(Shader::UsageHint::Default)].Get();

		return _fragmentShader[index].Get();
	}

	Shader *Material::DrawSnapshot::GetVertexShader(Shader::UsageHint type) const
	{
		uint8 index = static_cast<uint8>(type);
		if(!_vertexShader[index].Get())
			return _vertexShader[static_cast<uint8>(Shader::UsageHint::Default)].Get();

		return _vertexShader[index].Get();
	}

	void Material::DrawSnapshot::SetTextures(const Array *textures)
	{
		Array *copy = SafeCopy(textures);
		_textures = copy;
		SafeRelease(copy);
	}

	void Material::DrawSnapshot::GetMergedProperties(Material *overrideMaterial, Properties &properties) const
	{
		Material::GetMergedProperties(_properties, _override, overrideMaterial, properties);
	}

	void Material::DrawSnapshot::GetMergedPipelineProperties(Material *overrideMaterial, PipelineProperties &properties) const
	{
		Material::GetMergedPipelineProperties(_pipelineProperties, _override, overrideMaterial, properties);
	}

	Material::Material(Shader *vertexShader, Shader *fragmentShader) :
		_override(0),
		_textures(new Array()),
		_skipRendering(false)
	{
		for(uint8 i = 0; i < static_cast<uint8>(Shader::UsageHint::COUNT); i++)
		{
			if(i == 0)
			{
				_vertexShader[i] = SafeRetain(vertexShader);
				_fragmentShader[i] = SafeRetain(fragmentShader);
			}
			else
			{
				_vertexShader[i] = nullptr;
				_fragmentShader[i] = nullptr;
			}

			RN_ASSERT(!_vertexShader[i] || _vertexShader[i]->GetType() == Shader::Type::Vertex, "Vertex shader must be a vertex shader");
			RN_ASSERT(!_fragmentShader[i] || _fragmentShader[i]->GetType() == Shader::Type::Fragment, "Fragment shader must be a fragment shader");
		}
	}

	Material::Material(const Material *other) :
		_override(other->_override),
		_textures(SafeCopy(other->_textures)),
		_skipRendering(other->_skipRendering),
		_properties(other->_properties),
		_pipelineProperties(other->_pipelineProperties)
	{
		for(uint8 i = 0; i < static_cast<uint8>(Shader::UsageHint::COUNT); i++)
		{
			_vertexShader[i] = SafeRetain(other->_vertexShader[i]);
			_fragmentShader[i] = SafeRetain(other->_fragmentShader[i]);
		}
	}

	Material::~Material()
	{
		SafeRelease(_textures);

		for(uint8 i = 0; i < static_cast<uint8>(Shader::UsageHint::COUNT); i++)
		{
			SafeRelease(_fragmentShader[i]);
			SafeRelease(_vertexShader[i]);
		}
	}

	Material *Material::WithShaders(Shader *vertexShader, Shader *fragmentShader)
	{
		Material *material = new Material(vertexShader, fragmentShader);
		return material->Autorelease();
	}

	Material *Material::WithMaterial(const Material *material)
	{
		Material *copyMaterial = new Material(material);
		return copyMaterial->Autorelease();
	}

	void Material::SetTextures(const Array *textures)
	{
		SafeRelease(_textures);
		_textures = textures->Copy();
	}

	void Material::AddTexture(Framebuffer *texture)
	{
		_textures->AddObject(texture);
	}

	void Material::AddTexture(Texture *texture)
	{
		_textures->AddObject(texture);
	}

	void Material::RemoveAllTextures()
	{
		_textures->RemoveAllObjects();
	}

	void Material::SetFragmentShader(Shader *shader, Shader::UsageHint type)
	{
		RN_ASSERT(shader, "A valid fragment shader needs to be assigned!");
		SafeRelease(_fragmentShader[type]);
		_fragmentShader[type] = SafeRetain(shader);
		RN_ASSERT(!_fragmentShader[type] || _fragmentShader[type]->GetType() == Shader::Type::Fragment, "Fragment shader must be a fragment shader");
	}

	void Material::SetVertexShader(Shader *shader, Shader::UsageHint type)
	{
		RN_ASSERT(shader, "A valid vertex shader needs to be assigned!");
		SafeRelease(_vertexShader[type]);
		_vertexShader[type] = SafeRetain(shader);
		RN_ASSERT(!_vertexShader[type] || _vertexShader[type]->GetType() == Shader::Type::Vertex, "Vertex shader must be a vertex shader");
	}

	void Material::SetOverride(Override override)
	{
		_override = override;
	}

	void Material::SetColorWriteMask(bool writeRed, bool writeGreen, bool writeBlue, bool writeAlpha)
	{
		_pipelineProperties.colorWriteMask = 0;

		if(writeRed)
			_pipelineProperties.colorWriteMask |= (1 << 0);
		if(writeGreen)
			_pipelineProperties.colorWriteMask |= (1 << 1);
		if(writeBlue)
			_pipelineProperties.colorWriteMask |= (1 << 2);
		if(writeAlpha)
			_pipelineProperties.colorWriteMask |= (1 << 3);
	}

	void Material::SetDepthWriteEnabled(bool depthWrite)
	{
		_pipelineProperties.depthWriteEnabled = depthWrite;
	}
	void Material::SetDepthMode(DepthMode mode)
	{
		_pipelineProperties.depthMode = mode;
	}

	void Material::SetTextureTileFactor(float factor)
	{
		_properties.textureTileFactor = factor;
	}

	void Material::SetAmbientColor(const Color &color)
	{
		_properties.ambientColor = color;
	}
	void Material::SetDiffuseColor(const Color &color)
	{
		_properties.diffuseColor = color;
	}
	void Material::SetSpecularColor(const Color &color)
	{
		_properties.specularColor = color;
	}
	void Material::SetEmissiveColor(const Color &color)
	{
		_properties.emissiveColor = color;
	}

	void Material::SetCustomMatrix1(const Matrix &matrix)
	{
		_properties.customMatrix1 = matrix;
	}
	void Material::SetCustomMatrix2(const Matrix &matrix)
	{
		_properties.customMatrix2 = matrix;
	}

	void Material::SetCullMode(CullMode mode)
	{
		_pipelineProperties.cullMode = mode;
	}

	void Material::SetPolygonOffset(bool enable, float factor, float units)
	{
		_pipelineProperties.usePolygonOffset = enable;
		_pipelineProperties.polygonOffsetFactor = -factor;
		_pipelineProperties.polygonOffsetUnits = -units;
	}

	void Material::SetAlphaToCoverage(bool enabled, float min, float max)
	{
		_pipelineProperties.useAlphaToCoverage = enabled;
		_properties.alphaToCoverageClamp.x = min;
		_properties.alphaToCoverageClamp.y = max;
	}

	void Material::SetUIClippingRect(Vector4 rect)
	{
		_properties.uiClippingRect = rect;
	}

	void Material::SetUIOffset(Vector2 offset)
	{
		_properties.uiOffset = offset;
	}

	void Material::SetUIOutlineColor(Color color)
	{
		_properties.uiOutlineColor = color;
	}

	void Material::SetBlendOperation(BlendOperation blendOperationRGB, BlendOperation blendOperationAlpha)
	{
		RN_ASSERT((blendOperationRGB != BlendOperation::None && blendOperationAlpha != BlendOperation::None) || blendOperationAlpha == blendOperationRGB, "Blend operation None can not be mixed with any of the others.");
		_pipelineProperties.blendOperationRGB = blendOperationRGB;
		_pipelineProperties.blendOperationAlpha = blendOperationAlpha;
	}

	void Material::SetBlendFactorSource(BlendFactor blendFactorRGB, BlendFactor blendFactorAlpha)
	{
		_pipelineProperties.blendFactorSourceRGB = blendFactorRGB;
		_pipelineProperties.blendFactorSourceAlpha = blendFactorAlpha;
	}

	void Material::SetBlendFactorDestination(BlendFactor blendFactorRGB, BlendFactor blendFactorAlpha)
	{
		_pipelineProperties.blendFactorDestinationRGB = blendFactorRGB;
		_pipelineProperties.blendFactorDestinationAlpha = blendFactorAlpha;
	}

	void Material::SetCustomShaderUniform(const String *name, Value *value)
	{
		_properties.SetCustomShaderUniform(name, value);
	}

	void Material::SetCustomShaderUniform(const String *name, Number *number)
	{
		_properties.SetCustomShaderUniform(name, number);
	}

	Object *Material::GetCustomShaderUniform(const String *name) const
	{
		return _properties.GetCustomShaderUniform(name);
	}

	void Material::SetSkipRendering(bool skip)
	{
		_skipRendering = skip;
	}

	Shader *Material::GetFragmentShader(Shader::UsageHint type) const
	{
		if(!_fragmentShader[static_cast<uint8>(type)] && !_vertexShader[static_cast<uint8>(type)])
			return _fragmentShader[static_cast<uint8>(Shader::UsageHint::Default)];

		return _fragmentShader[static_cast<uint8>(type)];
	}
	Shader *Material::GetVertexShader(Shader::UsageHint type) const
	{
		if(!_vertexShader[static_cast<uint8>(type)])
			return _vertexShader[static_cast<uint8>(Shader::UsageHint::Default)];

		return _vertexShader[static_cast<uint8>(type)];
	}

	const Material::Properties &Material::GetMergedProperties(Material *overrideMaterial)
	{
		if(!overrideMaterial) return _properties;

		GetMergedProperties(_properties, _override, overrideMaterial, _mergedProperties);
		return _mergedProperties;
	}

	void Material::GetMergedProperties(const Properties &properties, uint32 override, Material *overrideMaterial, Properties &result)
	{
		if(!overrideMaterial)
		{
			result.CopyFromProperties(properties);
			return;
		}

		if(!(overrideMaterial->GetOverride() & Override::GroupColors) && !(override & Override::GroupColors))
		{
			result.ambientColor = overrideMaterial->_properties.ambientColor;
			result.diffuseColor = overrideMaterial->_properties.diffuseColor;
			result.specularColor = overrideMaterial->_properties.specularColor;
			result.emissiveColor = overrideMaterial->_properties.emissiveColor;
		}
		else
		{
			result.ambientColor = properties.ambientColor;
			result.diffuseColor = properties.diffuseColor;
			result.specularColor = properties.specularColor;
			result.emissiveColor = properties.emissiveColor;
		}

		if(!(overrideMaterial->GetOverride() & Override::GroupAlphaToCoverage) && !(override & Override::GroupAlphaToCoverage))
		{
			result.alphaToCoverageClamp = overrideMaterial->_properties.alphaToCoverageClamp;
		}
		else
		{
			result.alphaToCoverageClamp = properties.alphaToCoverageClamp;
		}

		if(!(overrideMaterial->GetOverride() & Override::TextureTileFactor) && !(override & Override::TextureTileFactor))
		{
			result.textureTileFactor = overrideMaterial->_properties.textureTileFactor;
		}
		else
		{
			result.textureTileFactor = properties.textureTileFactor;
		}

		result.customMatrix1 = properties.customMatrix1;
		result.customMatrix2 = properties.customMatrix2;

		result.uiClippingRect = properties.uiClippingRect;
		result.uiOffset = properties.uiOffset;
		result.uiOutlineColor = properties.uiOutlineColor;

		result.ClearCustomShaderUniforms();
		result._customShaderUniforms.insert(properties._customShaderUniforms.begin(), properties._customShaderUniforms.end());
		if(!(overrideMaterial->GetOverride() & Override::CustomUniforms) && !(override & Override::CustomUniforms) && overrideMaterial->_properties._customShaderUniforms.size() > 0)
		{
			for(auto const &data : overrideMaterial->_properties._customShaderUniforms)
			{
				result._customShaderUniforms[data.first] = data.second;
			}
		}
		for(auto const &data : result._customShaderUniforms)
		{
			data.second->Retain();
		}
	}

	const Material::PipelineProperties &Material::GetMergedPipelineProperties(Material *overrideMaterial)
	{
		if(!overrideMaterial) return _pipelineProperties;

		GetMergedPipelineProperties(_pipelineProperties, _override, overrideMaterial, _mergedPipelineProperties);
		return _mergedPipelineProperties;
	}

	void Material::GetMergedPipelineProperties(const PipelineProperties &properties, uint32 override, Material *overrideMaterial, PipelineProperties &result)
	{
		if(!overrideMaterial)
		{
			result.CopyFromPipelineProperties(properties);
			return;
		}

		if(!(overrideMaterial->GetOverride() & Override::ColorWriteMask) && !(override & Override::ColorWriteMask))
		{
			result.colorWriteMask = overrideMaterial->_pipelineProperties.colorWriteMask;
		}
		else
		{
			result.colorWriteMask = properties.colorWriteMask;
		}

		if(!(overrideMaterial->GetOverride() & Override::DepthWrite) && !(override & Override::DepthWrite))
		{
			result.depthWriteEnabled = overrideMaterial->_pipelineProperties.depthWriteEnabled;
		}
		else
		{
			result.depthWriteEnabled = properties.depthWriteEnabled;
		}

		if(!(overrideMaterial->GetOverride() & Override::GroupDepth) && !(override & Override::GroupDepth))
		{
			result.depthMode = overrideMaterial->_pipelineProperties.depthMode;
		}
		else
		{
			result.depthMode = properties.depthMode;
		}

		if(!(overrideMaterial->GetOverride() & Override::GroupAlphaToCoverage) && !(override & Override::GroupAlphaToCoverage))
		{
			result.useAlphaToCoverage = overrideMaterial->_pipelineProperties.useAlphaToCoverage;
		}
		else
		{
			result.useAlphaToCoverage = properties.useAlphaToCoverage;
		}

		if(!(overrideMaterial->GetOverride() & Override::GroupBlending) && !(override & Override::GroupBlending))
		{
			result.blendOperationRGB = overrideMaterial->_pipelineProperties.blendOperationRGB;
			result.blendOperationAlpha = overrideMaterial->_pipelineProperties.blendOperationAlpha;
			result.blendFactorSourceRGB = overrideMaterial->_pipelineProperties.blendFactorSourceRGB;
			result.blendFactorSourceAlpha = overrideMaterial->_pipelineProperties.blendFactorSourceAlpha;
			result.blendFactorDestinationRGB = overrideMaterial->_pipelineProperties.blendFactorDestinationRGB;
			result.blendFactorDestinationAlpha = overrideMaterial->_pipelineProperties.blendFactorDestinationAlpha;
		}
		else
		{
			result.blendOperationRGB = properties.blendOperationRGB;
			result.blendOperationAlpha = properties.blendOperationAlpha;
			result.blendFactorSourceRGB = properties.blendFactorSourceRGB;
			result.blendFactorSourceAlpha = properties.blendFactorSourceAlpha;
			result.blendFactorDestinationRGB = properties.blendFactorDestinationRGB;
			result.blendFactorDestinationAlpha = properties.blendFactorDestinationAlpha;
		}

		if(!(overrideMaterial->GetOverride() & Override::GroupPolygonOffset) && !(override & Override::GroupPolygonOffset))
		{
			result.usePolygonOffset = overrideMaterial->_pipelineProperties.usePolygonOffset;
			result.polygonOffsetFactor = overrideMaterial->_pipelineProperties.polygonOffsetFactor;
			result.polygonOffsetUnits = overrideMaterial->_pipelineProperties.polygonOffsetUnits;
		}
		else
		{
			result.usePolygonOffset = properties.usePolygonOffset;
			result.polygonOffsetFactor = properties.polygonOffsetFactor;
			result.polygonOffsetUnits = properties.polygonOffsetUnits;
		}

		if(!(overrideMaterial->GetOverride() & Override::CullMode) && !(override & Override::CullMode))
		{
			result.cullMode = overrideMaterial->_pipelineProperties.cullMode;
		}
		else
		{
			result.cullMode = properties.cullMode;
		}
	}

	void Material::GetDrawSnapshot(DrawSnapshot &snapshot) const
	{
		snapshot._override = _override;
		snapshot.SetTextures(_textures);

		snapshot._properties.CopyFromProperties(_properties);
		snapshot._pipelineProperties.CopyFromPipelineProperties(_pipelineProperties);

		for(uint8 i = 0; i < static_cast<uint8>(Shader::UsageHint::COUNT); i++)
		{
			snapshot._vertexShader[i] = _vertexShader[i];
			snapshot._fragmentShader[i] = _fragmentShader[i];
		}
	}
} // namespace RN
