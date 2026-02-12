//
//  RNFlexMeasure.h
//  Rayne
//
//  Intrinsic size adapter for Yoga.
//

#ifndef __RN_FLEX_MEASURE_H_
#define __RN_FLEX_MEASURE_H_

#include <Rayne.h>
#include <yoga/Yoga.h>

namespace RN
{
	class FlexMeasure
	{
	public:
		virtual ~FlexMeasure() = default;
		virtual YGSize Measure(float width, YGMeasureMode widthMode, float height, YGMeasureMode heightMode) = 0;

	protected:
		static float ResolveSize(float content, float constraint, YGMeasureMode mode)
		{
			if(mode == YGMeasureModeUndefined || YGFloatIsUndefined(constraint)) return content;
			if(mode == YGMeasureModeExactly) return constraint;
			return std::min(content, constraint);
		}
	};
} // namespace RN

#endif /* defined(__RN_FLEX_MEASURE_H_) */
