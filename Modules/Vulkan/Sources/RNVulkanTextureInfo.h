//
//  RNVulkanTextureInfo.h
//  Rayne
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_VULKANTEXTUREINFO_H_
#define __RAYNE_VULKANTEXTUREINFO_H_

#include "RNVulkan.h"

namespace RN
{
	namespace VulkanTextureInfo
	{
		struct FormatInfo
		{
			VkFormat format;
			VkImageAspectFlags aspectMask;
			uint8 blockWidth;
			uint8 blockHeight;
			uint8 bytesPerBlock;
			bool isDepth;
			bool isStencil;
		};

		const FormatInfo &GetFormatInfo(Texture::Format format);
		VkFormat GetFormat(Texture::Format format);
		VkImageAspectFlags GetAspectMask(Texture::Format format);
		VkImageLayout GetReadOnlyLayout(Texture::Format format);
		VkImageLayout GetRenderTargetLayout(Texture::Format format);
		VkImageType GetImageType(Texture::Type type);
		VkImageViewType GetImageViewType(Texture::Type type);
		uint32 GetImageLayerCount(const Texture::Descriptor &descriptor);
	}
}

#endif /* __RAYNE_VULKANTEXTUREINFO_H_ */
