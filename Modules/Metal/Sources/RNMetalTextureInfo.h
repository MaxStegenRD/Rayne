//
//  RNMetalTextureInfo.h
//  Rayne
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_METALTEXTUREINFO_H_
#define __RAYNE_METALTEXTUREINFO_H_

#include "RNMetal.h"

namespace RN
{
	namespace MetalTextureInfo
	{
		struct FormatInfo
		{
			MTLPixelFormat pixelFormat;
			uint8 blockWidth;
			uint8 blockHeight;
			bool isDepth;
			bool isStencil;
		};

		const FormatInfo &GetFormatInfo(Texture::Format format);
		MTLPixelFormat GetPixelFormat(Texture::Format format);
		uint32 GetBlockWidth(Texture::Format format);
		uint32 GetBlockHeight(Texture::Format format);
		MTLTextureDescriptor *CreateTextureDescriptor(const Texture::Descriptor &descriptor, bool isIOSurfaceBacked = false);
	}
}

#endif /* __RAYNE_METALTEXTUREINFO_H_ */
