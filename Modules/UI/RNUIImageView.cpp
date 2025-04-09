//
//  RNUIImageView.cpp
//  Rayne
//
//  Copyright 2017 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNUIImageView.h"

namespace RN
{
	namespace UI
	{
		RNDefineMeta(ImageView, View)

		ImageView::ImageView() :
			_framebuffer(nullptr), _image(nullptr), _color(Color::White())
		{
		}

		ImageView::ImageView(Texture *image) :
				_framebuffer(nullptr), _image(nullptr), _color(Color::White())
		{
			SetImage(image);
		}

		ImageView::ImageView(Framebuffer *framebuffer) :
				_framebuffer(framebuffer->Retain()), _image(nullptr), _color(Color::White())
		{

		}

		ImageView::~ImageView()
		{
			SafeRelease(_image);
		}

		void ImageView::SetImage(Texture *image)
		{
			if(_image == image && !_framebuffer) return;

			SafeRelease(_framebuffer);
			SafeRelease(_image);
			_image = SafeRetain(image);

			Model *model = GetModel();
			if(model)
			{
				Material *material = model->GetLODStage(0)->GetMaterialAtIndex(1);
				material->RemoveAllTextures();
				if(_image) material->AddTexture(_image);

				Color finalColor = _color;
				finalColor.a *= _combinedOpacityFactor;
				material->SetSkipRendering(_image == nullptr || finalColor.a < k::EpsilonFloat);
			}
		}
	
		void ImageView::SetFramebuffer(Framebuffer *framebuffer)
		{
			if(_framebuffer == framebuffer && !_image) return;

			SafeRelease(_image);
			SafeRelease(_framebuffer);
			_framebuffer = SafeRetain(framebuffer);

			Model *model = GetModel();
			if(model)
			{
				Material *material = model->GetLODStage(0)->GetMaterialAtIndex(1);
				material->RemoveAllTextures();
				if(_framebuffer) material->AddTexture(_framebuffer);

				Color finalColor = _color;
				finalColor.a *= _combinedOpacityFactor;
				material->SetSkipRendering(_framebuffer == nullptr || finalColor.a < k::EpsilonFloat);
			}
		}

		void ImageView::SetColor(Color color)
		{
			_color = color;

			Model *model = GetModel();
			if(model)
			{
				Material *material = model->GetLODStage(0)->GetMaterialAtIndex(1);
				Color finalColor = _color;
				finalColor.a *= _combinedOpacityFactor;
				material->SetDiffuseColor(finalColor);
				material->SetSkipRendering(_image == nullptr || finalColor.a < k::EpsilonFloat);
			}
		}

		void ImageView::UpdateModel()
		{
			View::UpdateModel();
			RN::Model::LODStage *lodStage = GetModel()->GetLODStage(0);

			if(lodStage->GetCount() < 2)
			{
				RN::Shader::Options *shaderOptions = RN::Shader::Options::WithNone();
				shaderOptions->EnableAlpha();
				shaderOptions->AddDefine("RN_UI", "1");
				shaderOptions->AddDefine("RN_UV0", "1");
				if(GetCornerRadius().x > 0.0f || GetCornerRadius().y > 0.0f || GetCornerRadius().z > 0.0f || GetCornerRadius().w > 0.0f) shaderOptions->AddDefine("RN_UV1", "1");

				RN::Material *material = RN::Material::WithShaders(Renderer::GetActiveRenderer()->GetDefaultShader(Shader::Type::Vertex, shaderOptions), Renderer::GetActiveRenderer()->GetDefaultShader(Shader::Type::Fragment, shaderOptions));
				material->SetVertexShader(Renderer::GetActiveRenderer()->GetDefaultShader(Shader::Type::Vertex, shaderOptions, RN::Shader::UsageHint::Multiview), RN::Shader::UsageHint::Multiview);
				material->SetFragmentShader(Renderer::GetActiveRenderer()->GetDefaultShader(Shader::Type::Fragment, shaderOptions, RN::Shader::UsageHint::Multiview), RN::Shader::UsageHint::Multiview);
				material->SetAlphaToCoverage(false);
				material->SetDepthMode(_depthMode);
				material->SetDepthWriteEnabled(false);
				material->SetCullMode(CullMode::None);
				material->SetBlendOperation(BlendOperation::Add, BlendOperation::Max);
				material->SetBlendFactorSource(BlendFactor::SourceAlpha, BlendFactor::One);
				material->SetBlendFactorDestination(BlendFactor::OneMinusSourceAlpha, BlendFactor::One);

				Color finalColor = _color;
				finalColor.a *= _combinedOpacityFactor;
				material->SetDiffuseColor(finalColor);

				const Rect &scissorRect = GetScissorRect();
				material->SetUIClippingRect(Vector4(scissorRect.GetLeft(), scissorRect.GetRight(), scissorRect.GetTop(), scissorRect.GetBottom()));
				material->SetUIOffset(Vector2(0.0f, 0.0f));

				lodStage->AddMesh(lodStage->GetMeshAtIndex(0), material);

				if(_framebuffer) material->AddTexture(_framebuffer);
				else if(_image) material->AddTexture(_image);
				material->SetSkipRendering((_image == nullptr && _framebuffer == nullptr) || finalColor.a < k::EpsilonFloat);

				Model *model = GetModel();
				model->Retain();
				SetModel(model);
				model->Release();
			}
			else
			{
				lodStage->ReplaceMesh(lodStage->GetMeshAtIndex(0), 1);
			}
		}

		void ImageView::SetOpacityFromParent(float parentCombinedOpacity)
		{
			View::SetOpacityFromParent(parentCombinedOpacity);
			Lock();

			Model *model = GetModel();
			if(model)
			{
				Material *material = model->GetLODStage(0)->GetMaterialAtIndex(1);
				Color finalColor = _color;
				finalColor.a *= _combinedOpacityFactor;
				material->SetDiffuseColor(finalColor);
				material->SetSkipRendering(_image == nullptr || finalColor.a < k::EpsilonFloat);
			}

			Unlock();
		}
	} // namespace UI
} // namespace RN
