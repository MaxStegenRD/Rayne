//
//  RNUIStackView.h
//  Rayne
//
//  Copyright 2024 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_UISTACKVIEW_H_
#define __RAYNE_UISTACKVIEW_H_

#include "RNUIView.h"

namespace RN
{
	namespace UI
	{
		class StackViewProtocol
		{
		public:
			virtual void StackViewViewChangedTransitionState(View *view, bool isTransitioning) = 0;
			virtual void StackViewViewChangedVisibility(View *view, bool isVisible) = 0;
		};

		class StackView : public View
		{
		public:
			enum AnimationType
			{
				AnimationTypeNone,
				AnimationTypeFade,
				AnimationTypeSlide
			};

			UIAPI StackView(StackViewProtocol *delegate = nullptr);
			UIAPI StackView(const Rect &frame, StackViewProtocol *delegate = nullptr);
			UIAPI ~StackView();

			UIAPI void SetFrame(const Rect &frame) override;
			UIAPI void SetDelegate(StackViewProtocol *delegate);

			UIAPI void Update(float delta) override;

			UIAPI void ReplaceTopView(View *view, AnimationType animationType = AnimationTypeNone);
			UIAPI void Push(View *view, AnimationType animationType = AnimationTypeNone, int32 renderPriorityOffset = 0);
			UIAPI void Pop(AnimationType animationType = AnimationTypeNone);
			UIAPI View *GetTopView() const;

			Array *GetViewStack() const { return _viewStack; }

			UIAPI bool GetIsAnimating() const;

		private:
			Array *_viewStack;
			View *_containerView;

			StackViewProtocol *_delegate;

			AnimationType _currentPushAnimationType;
			AnimationType _currentPopAnimationType;
			float _pushPopAnimationFactor;
			bool _isReplace;

			RNDeclareMetaAPI(StackView, UIAPI)
		};
	} // namespace UI
} // namespace RN


#endif /* __RAYNE_UISTACKVIEW_H_ */
