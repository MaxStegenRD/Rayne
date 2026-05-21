//
//  RNUIDebugTextBackend.cpp
//  Rayne-UI
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNUIDebugTextBackend.h"
#include "RNUILabel.h"
#include "RNUIWindow.h"

namespace RN
{
	namespace UI
	{
		RNDefineMeta(DebugTextBackend, RN::DebugTextBackend)

		struct DebugTextBackend::Internals
		{
			struct Entry
			{
				SceneNode *root = nullptr;
				float remaining = 0.0f;
				bool rendered = false;
			};

			Font *font = nullptr;
			float fontSize = 64.0f;
			float lineHeight = 0.08f;
			std::vector<Entry> entries;

			void RemoveEntry(size_t index)
			{
				Entry &entry = entries[index];
				if(entry.root)
				{
					if(entry.root->GetSceneInfo())
						entry.root->GetSceneInfo()->GetScene()->RemoveNode(entry.root);

					entry.root->Release();
				}

				entries.erase(entries.begin() + index);
			}
		};

		DebugTextBackend::DebugTextBackend(Font *font, float fontSize, float lineHeight) :
			_internals(new Internals())
		{
			_internals->font = SafeRetain(font);
			SetFontSize(fontSize);
			SetLineHeight(lineHeight);
		}

		DebugTextBackend::DebugTextBackend(String *fontPath, float fontSize, float lineHeight) :
			DebugTextBackend(fontPath ? FontManager::GetSharedInstance()->GetFontForFilepath(fontPath) : nullptr, fontSize, lineHeight)
		{
		}

		DebugTextBackend::~DebugTextBackend()
		{
			Clear();
			SafeRelease(_internals->font);
			delete _internals;
		}

		void DebugTextBackend::Clear()
		{
			Lock();
			while(!_internals->entries.empty())
				_internals->RemoveEntry(_internals->entries.size() - 1);
			Unlock();
		}

		void DebugTextBackend::Update(float delta)
		{
			Lock();
			for(size_t i = 0; i < _internals->entries.size();)
			{
				Internals::Entry &entry = _internals->entries[i];
				if(entry.remaining <= 0.0f || !entry.rendered)
				{
					i += 1;
					continue;
				}

				entry.remaining -= delta;
				if(entry.remaining <= 0.0f)
				{
					_internals->RemoveEntry(i);
					continue;
				}

				i += 1;
			}
			Unlock();
		}

		void DebugTextBackend::WillRender(Renderer *, Camera *camera)
		{
			if(!camera)
				return;

			Lock();
			for(Internals::Entry &entry : _internals->entries)
			{
				if(entry.root)
					entry.root->SetWorldRotation(camera->GetWorldRotation());
			}
			Unlock();
		}

		void DebugTextBackend::DidRender(Renderer *)
		{
			Lock();
			for(size_t i = 0; i < _internals->entries.size();)
			{
				Internals::Entry &entry = _internals->entries[i];
				if(entry.remaining <= 0.0f)
				{
					_internals->RemoveEntry(i);
					continue;
				}

				entry.rendered = true;
				i += 1;
			}
			Unlock();
		}

		void DebugTextBackend::DrawText(Scene *scene, const Vector3 &position, const String *text, const DebugDrawOptions &options)
		{
			if(!scene || !_internals->font || !_internals->font->IsValid() || !text || Renderer::IsHeadless())
				return;

			const float fontSize = std::max(_internals->fontSize, 1.0f);
			const float lineHeight = std::max(_internals->lineHeight, 0.001f);
			const float scale = lineHeight / fontSize;

			TextAttributes attributes(_internals->font, fontSize, options.color, TextAlignmentCenter);
			Label *label = new Label(attributes);
			label->SetBackgroundColor(Color::ClearColor());
			label->SetVerticalAlignment(TextVerticalAlignmentCenter);
			label->SetTextDepthMode(options.depthTest ? DepthMode::Greater : DepthMode::Always);
			label->SetText(text);

			Vector2 textSize = label->GetTextSize();
			textSize.x = std::max(textSize.x, 1.0f);
			textSize.y = std::max(textSize.y, fontSize);
			label->SetFrame(Rect(0.0f, 0.0f, textSize.x, textSize.y));

			Window *window = new Window(Rect(0.0f, 0.0f, textSize.x, textSize.y));
			window->SetBackgroundColor(Color::ClearColor());
			window->SetDepthModeAndWrite(options.depthTest ? DepthMode::Greater : DepthMode::Always, false, 0.0f, 0.0f, true, true);
			window->SetRenderPriority(SceneNode::RenderPriority::RenderUI + 10000);
			window->SetScale(Vector3(scale, scale, scale));
			window->SetPosition(Vector3(textSize.x * scale * -0.5f, textSize.y * scale * 0.5f, 0.0f));
			window->AddFlags(SceneNode::Flags::NoCulling);
			window->AddSubview(label);
			label->Release();

			SceneNode *root = new SceneNode(position);
			root->SetUpdatePriority(SceneNode::UpdatePriority::UpdateNever);
			root->SetFlags(SceneNode::Flags::NoCulling);
			root->AddChild(window);
			window->Release();

			scene->AddNode(root);

			Internals::Entry entry;
			entry.root = root;
			entry.remaining = options.duration;
			entry.rendered = false;

			Lock();
			_internals->entries.push_back(entry);
			Unlock();
		}

		void DebugTextBackend::SetFont(Font *font)
		{
			SafeRelease(_internals->font);
			_internals->font = SafeRetain(font);
		}

		Font *DebugTextBackend::GetFont() const
		{
			return _internals->font;
		}

		void DebugTextBackend::SetFontSize(float fontSize)
		{
			_internals->fontSize = std::max(fontSize, 1.0f);
		}

		void DebugTextBackend::SetLineHeight(float lineHeight)
		{
			_internals->lineHeight = std::max(lineHeight, 0.001f);
		}
	} // namespace UI
} // namespace RN
