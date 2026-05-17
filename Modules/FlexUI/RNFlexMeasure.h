//
//  RNFlexMeasure.h
//  Rayne
//
//  Intrinsic size adapter for FlexUI.
//

#ifndef __RN_FLEX_MEASURE_H_
#define __RN_FLEX_MEASURE_H_

#include <Rayne.h>
#include "RNFlexConfig.h"

namespace RN
{
	enum class FlexMeasureMode
	{
		Undefined,
		Exactly,
		AtMost
	};

	class FlexMeasure
	{
	public:
		FLXAPI virtual ~FlexMeasure() = default;
		FLXAPI virtual RN::Vector2 Measure(float width, FlexMeasureMode widthMode, float height, FlexMeasureMode heightMode) = 0;

	protected:
		static bool IsUndefined(float value)
		{
			return value != value;
		}

		static float ResolveSize(float content, float constraint, FlexMeasureMode mode)
		{
			if(mode == FlexMeasureMode::Undefined || IsUndefined(constraint)) return content;
			if(mode == FlexMeasureMode::Exactly) return constraint;
			return std::min(content, constraint);
		}
	};
} // namespace RN

#endif /* defined(__RN_FLEX_MEASURE_H_) */
