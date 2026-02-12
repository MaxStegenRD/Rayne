//
//  RNFlexStyle.h
//  Rayne
//
//  Minimal flex layout types for FlexUI.
//

#ifndef __RN_FLEX_STYLE_H_
#define __RN_FLEX_STYLE_H_

#include <Rayne.h>

namespace RN
{
	enum class FlexDirection
	{
		Row,
		Column
	};

	enum class FlexJustify
	{
		Start,
		Center,
		End,
		SpaceBetween,
		SpaceAround
	};

	enum class FlexAlign
	{
		Start,
		Center,
		End,
		Stretch
	};

	struct FlexStyle
	{
		// Size (set to negative to use intrinsic size)
		float width = -1.0f;
		float height = -1.0f;
		float minWidth = -1.0f;
		float minHeight = -1.0f;
		float maxWidth = -1.0f;
		float maxHeight = -1.0f;

		// Flex
		float flexGrow = 0.0f;
		float flexShrink = 0.0f;
		float flexBasis = -1.0f;

		// Margins
		float marginLeft = 0.0f;
		float marginTop = 0.0f;
		float marginRight = 0.0f;
		float marginBottom = 0.0f;
	};
} // namespace RN

#endif /* defined(__RN_FLEX_STYLE_H_) */
