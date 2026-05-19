//
//  OpenVRMask.hlsl
//  Rayne
//
//  Copyright 2017 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "rayne.hlsl"

#if RN_USE_MULTIVIEW
cbuffer vertexUniforms
{
	uint maskEyeIndex;
};
#endif

struct InputVertex
{
	float3 position : POSITION;

#if RN_USE_MULTIVIEW
	uint viewIndex : SV_VIEWID;
#endif
};

struct FragmentVertex
{
	float4 position : SV_POSITION;
};

FragmentVertex pp_mask_vertex(InputVertex vert)
{
	FragmentVertex result;

#if RN_USE_MULTIVIEW
	if(vert.viewIndex != maskEyeIndex)
	{
		result.position = float4(-2.0, -2.0, 1.0, 1.0);
		return result;
	}
#endif

	result.position = float4(vert.position.xy * 2.0 - 1.0, 1.0, 1.0);

	return result;
}

float4 pp_mask_fragment(FragmentVertex vert) : SV_TARGET
{
	return float4(1.0, 1.0, 1.0, 1.0);
}
