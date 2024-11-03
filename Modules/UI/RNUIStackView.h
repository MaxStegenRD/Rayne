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
			
			UIAPI void Update(float delta) override;
			
			UIAPI void Push(View *view, AnimationType animationType = AnimationTypeNone);
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

			RNDeclareMetaAPI(StackView, UIAPI)
		};
	}
}


#endif /* __RAYNE_UISTACKVIEW_H_ */
