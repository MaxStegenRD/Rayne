//
//  RNVulkanTextureInfo.cpp
//  Rayne
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNVulkanTextureInfo.h"

namespace RN
{
	namespace VulkanTextureInfo
	{
		const FormatInfo &GetFormatInfo(Texture::Format format)
		{
			static const FormatInfo formatInfos[] = {
				{ VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, 4, false, false },
				{ VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, 4, false, false },
				{ VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, 4, false, false },
				{ VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, 4, false, false },
				{ VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, 4, false, false },
				{ VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, 4, false, false },
				{ VK_FORMAT_A2R10G10B10_UNORM_PACK32, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, 4, false, false },
				{ VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, 4, false, false },
				{ VK_FORMAT_B10G11R11_UFLOAT_PACK32, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, 4, false, false },
				{ VK_FORMAT_R8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, 1, false, false },
				{ VK_FORMAT_R8G8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, 2, false, false },
				{ VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, 4, false, false },
				{ VK_FORMAT_R16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, 2, false, false },
				{ VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, 4, false, false },
				{ VK_FORMAT_R16G16B16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, 6, false, false },
				{ VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, 8, false, false },
				{ VK_FORMAT_R32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, 4, false, false },
				{ VK_FORMAT_R32G32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, 8, false, false },
				{ VK_FORMAT_R32G32B32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, 12, false, false },
				{ VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, 16, false, false },
				{ VK_FORMAT_D16_UNORM, VK_IMAGE_ASPECT_DEPTH_BIT, 1, 1, 2, true, false },
				{ VK_FORMAT_X8_D24_UNORM_PACK32, VK_IMAGE_ASPECT_DEPTH_BIT, 1, 1, 4, true, false },
				{ VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT, 1, 1, 4, true, false },
				{ VK_FORMAT_S8_UINT, VK_IMAGE_ASPECT_STENCIL_BIT, 1, 1, 1, false, true },
				{ VK_FORMAT_D24_UNORM_S8_UINT, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 1, 1, 4, true, true },
				{ VK_FORMAT_D32_SFLOAT_S8_UINT, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 1, 1, 5, true, true },
				{ VK_FORMAT_BC1_RGB_SRGB_BLOCK, VK_IMAGE_ASPECT_COLOR_BIT, 4, 4, 8, false, false },
				{ VK_FORMAT_BC2_SRGB_BLOCK, VK_IMAGE_ASPECT_COLOR_BIT, 4, 4, 16, false, false },
				{ VK_FORMAT_BC3_SRGB_BLOCK, VK_IMAGE_ASPECT_COLOR_BIT, 4, 4, 16, false, false },
				{ VK_FORMAT_BC7_SRGB_BLOCK, VK_IMAGE_ASPECT_COLOR_BIT, 4, 4, 16, false, false },
				{ VK_FORMAT_BC1_RGB_UNORM_BLOCK, VK_IMAGE_ASPECT_COLOR_BIT, 4, 4, 8, false, false },
				{ VK_FORMAT_BC2_UNORM_BLOCK, VK_IMAGE_ASPECT_COLOR_BIT, 4, 4, 16, false, false },
				{ VK_FORMAT_BC3_UNORM_BLOCK, VK_IMAGE_ASPECT_COLOR_BIT, 4, 4, 16, false, false },
				{ VK_FORMAT_BC4_UNORM_BLOCK, VK_IMAGE_ASPECT_COLOR_BIT, 4, 4, 8, false, false },
				{ VK_FORMAT_BC5_UNORM_BLOCK, VK_IMAGE_ASPECT_COLOR_BIT, 4, 4, 16, false, false },
				{ VK_FORMAT_BC7_UNORM_BLOCK, VK_IMAGE_ASPECT_COLOR_BIT, 4, 4, 16, false, false },
				{ VK_FORMAT_ASTC_4x4_SRGB_BLOCK, VK_IMAGE_ASPECT_COLOR_BIT, 4, 4, 16, false, false },
				{ VK_FORMAT_ASTC_5x4_SRGB_BLOCK, VK_IMAGE_ASPECT_COLOR_BIT, 5, 4, 16, false, false },
				{ VK_FORMAT_ASTC_5x5_SRGB_BLOCK, VK_IMAGE_ASPECT_COLOR_BIT, 5, 5, 16, false, false },
				{ VK_FORMAT_ASTC_6x5_SRGB_BLOCK, VK_IMAGE_ASPECT_COLOR_BIT, 6, 5, 16, false, false },
				{ VK_FORMAT_ASTC_6x6_SRGB_BLOCK, VK_IMAGE_ASPECT_COLOR_BIT, 6, 6, 16, false, false },
				{ VK_FORMAT_ASTC_8x5_SRGB_BLOCK, VK_IMAGE_ASPECT_COLOR_BIT, 8, 5, 16, false, false },
				{ VK_FORMAT_ASTC_8x6_SRGB_BLOCK, VK_IMAGE_ASPECT_COLOR_BIT, 8, 6, 16, false, false },
				{ VK_FORMAT_ASTC_8x8_SRGB_BLOCK, VK_IMAGE_ASPECT_COLOR_BIT, 8, 8, 16, false, false },
				{ VK_FORMAT_ASTC_10x5_SRGB_BLOCK, VK_IMAGE_ASPECT_COLOR_BIT, 10, 5, 16, false, false },
				{ VK_FORMAT_ASTC_10x6_SRGB_BLOCK, VK_IMAGE_ASPECT_COLOR_BIT, 10, 6, 16, false, false },
				{ VK_FORMAT_ASTC_10x8_SRGB_BLOCK, VK_IMAGE_ASPECT_COLOR_BIT, 10, 8, 16, false, false },
				{ VK_FORMAT_ASTC_10x10_SRGB_BLOCK, VK_IMAGE_ASPECT_COLOR_BIT, 10, 10, 16, false, false },
				{ VK_FORMAT_ASTC_12x10_SRGB_BLOCK, VK_IMAGE_ASPECT_COLOR_BIT, 12, 10, 16, false, false },
				{ VK_FORMAT_ASTC_12x12_SRGB_BLOCK, VK_IMAGE_ASPECT_COLOR_BIT, 12, 12, 16, false, false },
				{ VK_FORMAT_ASTC_4x4_UNORM_BLOCK, VK_IMAGE_ASPECT_COLOR_BIT, 4, 4, 16, false, false },
				{ VK_FORMAT_ASTC_5x4_UNORM_BLOCK, VK_IMAGE_ASPECT_COLOR_BIT, 5, 4, 16, false, false },
				{ VK_FORMAT_ASTC_5x5_UNORM_BLOCK, VK_IMAGE_ASPECT_COLOR_BIT, 5, 5, 16, false, false },
				{ VK_FORMAT_ASTC_6x5_UNORM_BLOCK, VK_IMAGE_ASPECT_COLOR_BIT, 6, 5, 16, false, false },
				{ VK_FORMAT_ASTC_6x6_UNORM_BLOCK, VK_IMAGE_ASPECT_COLOR_BIT, 6, 6, 16, false, false },
				{ VK_FORMAT_ASTC_8x5_UNORM_BLOCK, VK_IMAGE_ASPECT_COLOR_BIT, 8, 5, 16, false, false },
				{ VK_FORMAT_ASTC_8x6_UNORM_BLOCK, VK_IMAGE_ASPECT_COLOR_BIT, 8, 6, 16, false, false },
				{ VK_FORMAT_ASTC_8x8_UNORM_BLOCK, VK_IMAGE_ASPECT_COLOR_BIT, 8, 8, 16, false, false },
				{ VK_FORMAT_ASTC_10x5_UNORM_BLOCK, VK_IMAGE_ASPECT_COLOR_BIT, 10, 5, 16, false, false },
				{ VK_FORMAT_ASTC_10x6_UNORM_BLOCK, VK_IMAGE_ASPECT_COLOR_BIT, 10, 6, 16, false, false },
				{ VK_FORMAT_ASTC_10x8_UNORM_BLOCK, VK_IMAGE_ASPECT_COLOR_BIT, 10, 8, 16, false, false },
				{ VK_FORMAT_ASTC_10x10_UNORM_BLOCK, VK_IMAGE_ASPECT_COLOR_BIT, 10, 10, 16, false, false },
				{ VK_FORMAT_ASTC_12x10_UNORM_BLOCK, VK_IMAGE_ASPECT_COLOR_BIT, 12, 10, 16, false, false },
				{ VK_FORMAT_ASTC_12x12_UNORM_BLOCK, VK_IMAGE_ASPECT_COLOR_BIT, 12, 12, 16, false, false },
				{ VK_FORMAT_UNDEFINED, 0, 1, 1, 0, false, false }
			};

			static_assert(sizeof(formatInfos) / sizeof(formatInfos[0]) == static_cast<size_t>(Texture::Format::Invalid) + 1, "Vulkan texture format table is out of sync");

			uint32 index = static_cast<uint32>(format);
			uint32 invalidIndex = static_cast<uint32>(Texture::Format::Invalid);
			if(index > invalidIndex)
				index = invalidIndex;

			return formatInfos[index];
		}

		VkFormat GetFormat(Texture::Format format)
		{
			return GetFormatInfo(format).format;
		}

		VkImageAspectFlags GetAspectMask(Texture::Format format)
		{
			return GetFormatInfo(format).aspectMask;
		}

		VkImageLayout GetReadOnlyLayout(Texture::Format format)
		{
			const FormatInfo &formatInfo = GetFormatInfo(format);
			return (formatInfo.isDepth || formatInfo.isStencil) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}

		VkImageLayout GetRenderTargetLayout(Texture::Format format)
		{
			const FormatInfo &formatInfo = GetFormatInfo(format);
			return (formatInfo.isDepth || formatInfo.isStencil) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}

		VkImageType GetImageType(Texture::Type type)
		{
			switch(type)
			{
				case Texture::Type::Type1D:
				case Texture::Type::Type1DArray:
					return VK_IMAGE_TYPE_1D;

				case Texture::Type::Type2D:
				case Texture::Type::Type2DArray:
				case Texture::Type::TypeCube:
				case Texture::Type::TypeCubeArray:
					return VK_IMAGE_TYPE_2D;

				case Texture::Type::Type3D:
					return VK_IMAGE_TYPE_3D;
			}

			throw InconsistencyException("Invalid texture type for Vulkan");
		}

		VkImageViewType GetImageViewType(Texture::Type type)
		{
			switch(type)
			{
				case Texture::Type::Type1D:
					return VK_IMAGE_VIEW_TYPE_1D;
				case Texture::Type::Type1DArray:
					return VK_IMAGE_VIEW_TYPE_1D_ARRAY;

				case Texture::Type::Type2D:
					return VK_IMAGE_VIEW_TYPE_2D;
				case Texture::Type::Type2DArray:
					return VK_IMAGE_VIEW_TYPE_2D_ARRAY;

				case Texture::Type::Type3D:
					return VK_IMAGE_VIEW_TYPE_3D;

				case Texture::Type::TypeCube:
					return VK_IMAGE_VIEW_TYPE_CUBE;
				case Texture::Type::TypeCubeArray:
					return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
			}

			throw InconsistencyException("Invalid texture type for Vulkan");
		}

		uint32 GetImageLayerCount(const Texture::Descriptor &descriptor)
		{
			return descriptor.type == Texture::Type::Type3D ? 1 : descriptor.depth;
		}
	}
}
