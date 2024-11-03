//
//  RNUIStackView.cpp
//  Rayne
//
//  Copyright 2024 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNUIStackView.h"

namespace RN
{
	namespace UI
	{
		RNDefineMeta(StackView, View)

		StackView::StackView(StackViewProtocol *delegate) : _delegate(delegate), _pushPopAnimationFactor(0.0f), _currentPopAnimationType(AnimationTypeNone), _currentPushAnimationType(AnimationTypeNone)
		{
			_viewStack = new Array();
			_containerView = new View();
			AddSubview(_containerView->Autorelease());
		}
	
		StackView::StackView(const Rect &frame, StackViewProtocol *delegate) : View(frame), _delegate(delegate), _pushPopAnimationFactor(0.0f), _currentPopAnimationType(AnimationTypeNone), _currentPushAnimationType(AnimationTypeNone)
		{
			_viewStack = new Array();
			_containerView = new View(GetBounds());
			AddSubview(_containerView->Autorelease());
		}
	
		StackView::~StackView()
		{
			SafeRelease(_viewStack);
		}
	
		void StackView::SetFrame(const Rect &frame)
		{
			View::SetFrame(frame);
			_containerView->SetFrame(GetBounds());
		}
	
		void StackView::Push(View *view, AnimationType animationType)
		{
			if(_viewStack->GetLastObject() == view) return;
			if(_currentPushAnimationType != AnimationTypeNone || _currentPopAnimationType != AnimationTypeNone) return;
			
			_currentPushAnimationType = animationType;
			_pushPopAnimationFactor = 0.0f;
			
			View *oldTopView = _viewStack->GetLastObject<View>();
			if(animationType == AnimationTypeNone && _viewStack->GetCount() > 0) //Only immediately remove the previous view if the transition is not animated
			{
				if(_delegate) _delegate->StackViewViewChangedVisibility(oldTopView, false);
				oldTopView->RemoveFromSuperview();
			}
			
			//This is a bit hacky cause the 20 is a bit of a magic value, but good enough for now
			view->SetRenderPriorityOffset(20 * _viewStack->GetCount() + 1); //Make sure the new view is displayed above all content of the previous view to prevent issues with the transition
			_viewStack->AddObject(view);
			_containerView->AddSubview(view);
			if(_delegate) _delegate->StackViewViewChangedVisibility(view, true);
			
			if(animationType == AnimationTypeFade)
			{
				view->SetOpacity(_pushPopAnimationFactor);
			}
			else if(animationType == AnimationTypeSlide)
			{
				RN::Rect newFrame = view->GetFrame();
				newFrame.y = newFrame.height * (1.0f - _pushPopAnimationFactor);
				view->SetFrame(newFrame);
			}
			
			if(animationType != AnimationTypeNone)
			{
				if(_delegate) _delegate->StackViewViewChangedTransitionState(view, true);
				if(_delegate) _delegate->StackViewViewChangedTransitionState(oldTopView, true);
			}
		}

		void StackView::Pop(AnimationType animationType)
		{
			if(_viewStack->GetCount() <= 1) return;
			if(_currentPushAnimationType != AnimationTypeNone || _currentPopAnimationType != AnimationTypeNone) return;
			
			_currentPopAnimationType = animationType;
			_pushPopAnimationFactor = 1.0f;
			
			View *view = _viewStack->GetLastObject<View>();
			view->RemoveFromSuperview(); //Need to keep the previous top view as top view, so remove, add previous view and put this one on top again
			
			View *newTopView = _viewStack->GetCount() > 1? _viewStack->GetObjectAtIndex<View>(_viewStack->GetCount() - 2) : nullptr;
			if(animationType == AnimationTypeNone) //Only immediately remove the popped view if the transition is not animated
			{
				if(_delegate) _delegate->StackViewViewChangedVisibility(view, false);
				_viewStack->RemoveObject(view);
				view = nullptr;
			}
			else
			{
				_containerView->AddSubview(view);
				if(_delegate) _delegate->StackViewViewChangedTransitionState(view, true);
				if(_delegate && newTopView) _delegate->StackViewViewChangedTransitionState(newTopView, true);
			}
			
			if(newTopView)
			{
				_containerView->AddSubview(newTopView);
				if(_delegate) _delegate->StackViewViewChangedVisibility(newTopView, true);
			}
			
			if(view && animationType == AnimationTypeFade)
			{
				view->SetOpacity(_pushPopAnimationFactor);
			}
			else if(view && animationType == AnimationTypeSlide)
			{
				RN::Rect newFrame = view->GetFrame();
				newFrame.y = newFrame.height * (1.0f - _pushPopAnimationFactor);
				view->SetFrame(newFrame);
			}
		}

		View *StackView::GetTopView() const
		{
			return _viewStack->GetLastObject<View>();
		}
	
		bool StackView::GetIsAnimating() const
		{
			return (_currentPushAnimationType != AnimationTypeNone || _currentPopAnimationType != AnimationTypeNone);
		}
	
		void StackView::Update(float delta)
		{
			View::Update(delta);
			
			View *animatedView = _viewStack->GetLastObject<View>();
			AnimationType previousAnimationType = AnimationTypeNone;
			if(_currentPushAnimationType != AnimationTypeNone)
			{
				previousAnimationType = _currentPushAnimationType;
				_pushPopAnimationFactor += delta * 4.0f;
				
				if(_pushPopAnimationFactor >= 1.0f)
				{
					View *oldTopView = _viewStack->GetCount() > 1? _viewStack->GetObjectAtIndex<View>(_viewStack->GetCount()-2) : nullptr;
					if(oldTopView)
					{
						if(_delegate) _delegate->StackViewViewChangedTransitionState(oldTopView, false);
						if(_delegate) _delegate->StackViewViewChangedVisibility(oldTopView, false);
						oldTopView->RemoveFromSuperview();
					}
					
					View *topView = _viewStack->GetFirstObject<View>();
					if(_delegate && topView) _delegate->StackViewViewChangedTransitionState(topView, false);
					
					_currentPushAnimationType = AnimationTypeNone;
				}
			}
			else if(_currentPopAnimationType != AnimationTypeNone)
			{
				previousAnimationType = _currentPopAnimationType;
				_pushPopAnimationFactor -= delta * 4.0f;
				if(_pushPopAnimationFactor <= 0.0f)
				{
					View *view = _viewStack->GetLastObject<View>();
					if(_delegate) _delegate->StackViewViewChangedTransitionState(view, false);
					if(_delegate) _delegate->StackViewViewChangedVisibility(view, false);
					view->RemoveFromSuperview();
					_viewStack->RemoveObject(view);
					animatedView = nullptr;
					
					View *newTopView = _viewStack->GetLastObject<View>();
					if(_delegate && newTopView) _delegate->StackViewViewChangedTransitionState(newTopView, false);
					
					_currentPopAnimationType = AnimationTypeNone;
				}
			}
			_pushPopAnimationFactor = std::min(1.0f, std::max(0.0f, _pushPopAnimationFactor));
			
			if(animatedView)
			{
				if(previousAnimationType == AnimationTypeFade)
				{
					animatedView->SetOpacity(_pushPopAnimationFactor);
				}
				else if(previousAnimationType == AnimationTypeSlide)
				{
					RN::Rect newFrame = animatedView->GetFrame();
					newFrame.y = newFrame.height * (1.0f - _pushPopAnimationFactor);
					animatedView->SetFrame(newFrame);
				}
			}
		}
	}
}
