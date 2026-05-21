//
//  RNUIDebugTextBackend.h
//  Rayne-UI
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_UIDEBUGTEXTBACKEND_H_
#define __RAYNE_UIDEBUGTEXTBACKEND_H_

#include "RNUIFontManager.h"

namespace RN
{
	namespace UI
	{
		class DebugTextBackend : public RN::DebugTextBackend
		{
		public:
			UIAPI DebugTextBackend(Font *font, float fontSize = 64.0f, float lineHeight = 0.08f);
			UIAPI DebugTextBackend(String *fontPath, float fontSize = 64.0f, float lineHeight = 0.08f);
			UIAPI ~DebugTextBackend() override;

			UIAPI void SetFont(Font *font);
			UIAPI Font *GetFont() const;

			UIAPI void SetFontSize(float fontSize);
			UIAPI void SetLineHeight(float lineHeight);

		private:
			UIAPI void Clear() override;
			UIAPI void Update(float delta) override;
			UIAPI void WillRender(Renderer *renderer, Camera *camera) override;
			UIAPI void DidRender(Renderer *renderer) override;
			UIAPI void DrawText(Scene *scene, const Vector3 &position, const String *text, const DebugDrawOptions &options) override;

			struct Internals;
			Internals *_internals;

			RNDeclareMetaAPI(DebugTextBackend, UIAPI)
		};
	} // namespace UI
} // namespace RN

#endif /* __RAYNE_UIDEBUGTEXTBACKEND_H_ */
