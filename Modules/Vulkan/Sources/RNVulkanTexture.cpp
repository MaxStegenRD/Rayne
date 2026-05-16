//
//  RNVulkanTexture.cpp
//  Rayne
//
//  Copyright 2016 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNVulkanTexture.h"
#include "RNVulkanTextureInfo.h"
#include "RNVulkanRenderer.h"
#include "RNVulkanInternals.h"

namespace RN
{
	RNDefineMeta(VulkanTexture, Texture)

	VkExternalMemoryHandleTypeFlagBits VulkanTexture::GetVulkanExternalMemoryHandleType(Texture::ExternalMemoryHandleType handleType)
	{
#if RN_PLATFORM_WINDOWS
		switch(handleType)
		{
			case Texture::ExternalMemoryHandleType::D3D11Texture:
				return VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT;
		}
#endif

		RN_ASSERT(false, "Unsupported external texture memory handle type");
		return static_cast<VkExternalMemoryHandleTypeFlagBits>(0);
	}

	VulkanTexture::VulkanTexture(const Descriptor &descriptor, VulkanRenderer *renderer) :
		Texture(descriptor),
		_renderer(renderer),
		_isStreamingData(false),
		_isFromSwapchain(false),
		_isExternalMemory(false),
		_format(VulkanTextureInfo::GetFormat(descriptor.format)),
		_image(VK_NULL_HANDLE),
		_imageView(VK_NULL_HANDLE),
		_allocation(VK_NULL_HANDLE),
		_externalMemory(VK_NULL_HANDLE),
		_currentUsage(LayoutUsage::Undefined)
	{
		CreateOwnedImage();
	}

	VulkanTexture::VulkanTexture(const Descriptor &descriptor, VulkanRenderer *renderer, VkImage image, bool fromSwapchain) :
		Texture(descriptor),
		_renderer(renderer),
		_isStreamingData(false),
		_isFromSwapchain(fromSwapchain),
		_isExternalMemory(false),
		_format(VulkanTextureInfo::GetFormat(descriptor.format)),
		_image(image),
		_imageView(VK_NULL_HANDLE),
		_allocation(VK_NULL_HANDLE),
		_externalMemory(VK_NULL_HANDLE),
		_currentUsage(LayoutUsage::Undefined)
	{
		RN_ASSERT(_format != VK_FORMAT_UNDEFINED, "Requested texture format is not supported by Vulkan (%i)", _descriptor.format);
		CreateImageView();
	}

	VulkanTexture::VulkanTexture(const Descriptor &descriptor, VulkanRenderer *renderer, const Texture::ExternalMemoryDescriptor &externalMemoryDescriptor) :
		Texture(descriptor),
		_renderer(renderer),
		_isStreamingData(false),
		_isFromSwapchain(false),
		_isExternalMemory(true),
		_format(VulkanTextureInfo::GetFormat(descriptor.format)),
		_image(VK_NULL_HANDLE),
		_imageView(VK_NULL_HANDLE),
		_allocation(VK_NULL_HANDLE),
		_externalMemory(VK_NULL_HANDLE),
		_currentUsage(LayoutUsage::Undefined)
	{
		CreateImageWithExternalMemory(externalMemoryDescriptor);
		CreateImageView();
	}

	void VulkanTexture::GetVulkanImageCreateInfo(VkImageCreateInfo &imageInfo) const
	{
		VulkanDevice *device = _renderer->GetVulkanDevice();
		const VulkanTextureInfo::FormatInfo &formatInfo = VulkanTextureInfo::GetFormatInfo(_descriptor.format);
		RN_ASSERT(formatInfo.format != VK_FORMAT_UNDEFINED, "Requested texture format is not supported by Vulkan (%i)", _descriptor.format);

		imageInfo = {};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.pNext = nullptr;
		imageInfo.imageType = VulkanTextureInfo::GetImageType(_descriptor.type);
		imageInfo.format = _format;
		const bool isDepthStencil = formatInfo.isDepth || formatInfo.isStencil;
		const uint32 imageDepth = _descriptor.type == Texture::Type::Type3D ? _descriptor.depth : 1;
		const uint32 imageLayers = VulkanTextureInfo::GetImageLayerCount(_descriptor);
		imageInfo.extent = { _descriptor.width, _descriptor.height, imageDepth };
		imageInfo.arrayLayers = imageLayers;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.samples = static_cast<VkSampleCountFlagBits>(_descriptor.sampleCount);
		imageInfo.usage = 0;
		imageInfo.flags = 0;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.mipLevels = _descriptor.mipMaps;

		if(_descriptor.sampleCount <= 1)
		{
			switch(_descriptor.accessOptions)
			{
				case GPUResource::AccessOptions::ReadWrite:
					imageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
					break;
				case GPUResource::AccessOptions::WriteOnly:
					imageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
					break;
				case GPUResource::AccessOptions::Private:
					imageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
					break;
			}
		}

		if(_descriptor.usageHint & Texture::UsageHint::RenderTarget || _descriptor.usageHint & Texture::UsageHint::InputAttachment)
		{
			imageInfo.usage |= isDepthStencil ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

			if(_descriptor.sampleCount > 1)
				imageInfo.usage |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
			else if(_descriptor.usageHint & Texture::UsageHint::RenderTarget)
				imageInfo.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

			if(_descriptor.usageHint & Texture::UsageHint::InputAttachment)
				imageInfo.usage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
		}
		else
		{
			imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		}

		if(_descriptor.usageHint & Texture::UsageHint::ShaderWrite)
			imageInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;

		if(_descriptor.type == Texture::Type::TypeCube || _descriptor.type == Texture::Type::TypeCubeArray)
		{
			RN_ASSERT(_descriptor.width == _descriptor.height, "Vulkan cube textures must be square");
			RN_ASSERT(imageLayers >= 6 && imageLayers % 6 == 0, "Vulkan cube textures need a layer count that is a multiple of six");
			imageInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		}

		if(_descriptor.usageHint & UsageHint::Subsampled && device->GetSupportsFragmentDensityMaps())
			imageInfo.flags |= VK_IMAGE_CREATE_SUBSAMPLED_BIT_EXT;
	}

	void VulkanTexture::ValidateImageCreateInfo(const VkImageCreateInfo &imageInfo) const
	{
		VulkanDevice *device = _renderer->GetVulkanDevice();

		VkImageFormatProperties formatProperties;
		RNVulkanValidate(vk::GetPhysicalDeviceImageFormatProperties(device->GetPhysicalDevice(), imageInfo.format, imageInfo.imageType, imageInfo.tiling, imageInfo.usage, imageInfo.flags, &formatProperties));

		VkFormatProperties properties;
		vk::GetPhysicalDeviceFormatProperties(device->GetPhysicalDevice(), _format, &properties);
		if(imageInfo.usage & VK_IMAGE_USAGE_SAMPLED_BIT)
			RN_ASSERT(properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT, "Requested texture format is not supported as a sampled image by this device (%i)", _descriptor.format);
		if(imageInfo.usage & VK_IMAGE_USAGE_STORAGE_BIT)
			RN_ASSERT(properties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT, "Requested texture format is not supported as a storage image by this device (%i)", _descriptor.format);
		if(imageInfo.usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
			RN_ASSERT(properties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT, "Requested texture format is not supported as a color attachment by this device (%i)", _descriptor.format);
		if(imageInfo.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
			RN_ASSERT(properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT, "Requested texture format is not supported as a depth/stencil attachment by this device (%i)", _descriptor.format);

		RN_ASSERT(formatProperties.sampleCounts & _descriptor.sampleCount, "Requested sample count for texture format is not supported by this device");
	}

	void VulkanTexture::CreateOwnedImage()
	{
		VkImageCreateInfo imageInfo;
		GetVulkanImageCreateInfo(imageInfo);

		VmaAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

		if(_descriptor.usageHint & UsageHint::RenderTarget)
		{
			allocCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			if(_descriptor.sampleCount > 1) allocCreateInfo.preferredFlags = VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
			allocCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
			allocCreateInfo.priority = 1.0f;
		}

		_currentUsage = LayoutUsage::Undefined;

		ValidateImageCreateInfo(imageInfo);

		RNVulkanValidate(vmaCreateImage(_renderer->_internals->memoryAllocator, &imageInfo, &allocCreateInfo, &_image, &_allocation, nullptr));

		if(_descriptor.usageHint & UsageHint::RenderTarget)
		{
			VulkanCommandBuffer *commandBuffer = _renderer->StartResourcesCommandBuffer();
			TransitionToUsage(commandBuffer->GetCommandBuffer(), LayoutUsage::RenderTarget);
			_renderer->EndResourcesCommandBuffer();
		}

		CreateImageView();
	}

	void VulkanTexture::CreateImageWithExternalMemory(const Texture::ExternalMemoryDescriptor &externalMemoryDescriptor)
	{
#if RN_PLATFORM_WINDOWS
		VulkanDevice *device = _renderer->GetVulkanDevice();
		RN_ASSERT(device->GetSupportsExternalTextureImport(), "Vulkan device does not support external texture import");
		RN_ASSERT(externalMemoryDescriptor.handle, "External Vulkan texture import requires a valid Win32 handle");
		RN_ASSERT(_format != VK_FORMAT_UNDEFINED, "Requested texture format is not supported by Vulkan (%i)", _descriptor.format);
		VkExternalMemoryHandleTypeFlagBits handleType = GetVulkanExternalMemoryHandleType(externalMemoryDescriptor.handleType);

		VkImageCreateInfo imageInfo;
		GetVulkanImageCreateInfo(imageInfo);

		VkExternalMemoryImageCreateInfo externalMemoryImageCreateInfo = {};
		externalMemoryImageCreateInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
		externalMemoryImageCreateInfo.pNext = imageInfo.pNext;
		externalMemoryImageCreateInfo.handleTypes = handleType;
		imageInfo.pNext = &externalMemoryImageCreateInfo;

		ValidateImageCreateInfo(imageInfo);

		VkPhysicalDeviceExternalImageFormatInfo externalImageFormatInfo = {};
		externalImageFormatInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO;
		externalImageFormatInfo.handleType = handleType;

		VkPhysicalDeviceImageFormatInfo2 imageFormatInfo = {};
		imageFormatInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
		imageFormatInfo.pNext = &externalImageFormatInfo;
		imageFormatInfo.format = imageInfo.format;
		imageFormatInfo.type = imageInfo.imageType;
		imageFormatInfo.tiling = imageInfo.tiling;
		imageFormatInfo.usage = imageInfo.usage;
		imageFormatInfo.flags = imageInfo.flags;

		VkExternalImageFormatProperties externalImageFormatProperties = {};
		externalImageFormatProperties.sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES;

		VkImageFormatProperties2 imageFormatProperties = {};
		imageFormatProperties.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
		imageFormatProperties.pNext = &externalImageFormatProperties;

		RNVulkanValidate(vk::GetPhysicalDeviceImageFormatProperties2(device->GetPhysicalDevice(), &imageFormatInfo, &imageFormatProperties));

		const VkExternalMemoryFeatureFlags externalMemoryFeatures = externalImageFormatProperties.externalMemoryProperties.externalMemoryFeatures;
		RN_ASSERT(externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT, "Requested Vulkan texture format can not import external memory");
		RN_ASSERT(externalMemoryDescriptor.dedicatedAllocation || !(externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT), "Requested Vulkan texture external memory import requires a dedicated allocation");
		RN_ASSERT(imageFormatProperties.imageFormatProperties.sampleCounts & _descriptor.sampleCount, "Requested sample count for external Vulkan texture format is not supported by this device");

		RNVulkanValidate(vk::CreateImage(device->GetDevice(), &imageInfo, _renderer->GetAllocatorCallback(), &_image));

		VkMemoryRequirements memoryRequirements = {};
		vk::GetImageMemoryRequirements(device->GetDevice(), _image, &memoryRequirements);

		VkMemoryWin32HandlePropertiesKHR handleProperties = {};
		handleProperties.sType = VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR;
		HANDLE win32Handle = reinterpret_cast<HANDLE>(externalMemoryDescriptor.handle);
		RNVulkanValidate(vk::GetMemoryWin32HandlePropertiesKHR(device->GetDevice(), handleType, win32Handle, &handleProperties));

		uint32 memoryTypeIndex = 0;
		RN_ASSERT(device->GetMemoryWithType(memoryRequirements.memoryTypeBits & handleProperties.memoryTypeBits, 0, memoryTypeIndex) == VK_TRUE, "No compatible memory type found for external Vulkan texture import");

		VkImportMemoryWin32HandleInfoKHR importMemoryInfo = {};
		importMemoryInfo.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR;
		importMemoryInfo.handleType = handleType;
		importMemoryInfo.handle = win32Handle;
		importMemoryInfo.name = nullptr;

		VkMemoryDedicatedAllocateInfo dedicatedAllocateInfo = {};
		dedicatedAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
		dedicatedAllocateInfo.pNext = &importMemoryInfo;
		dedicatedAllocateInfo.image = _image;
		dedicatedAllocateInfo.buffer = VK_NULL_HANDLE;

		VkMemoryAllocateInfo allocateInfo = {};
		allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocateInfo.pNext = externalMemoryDescriptor.dedicatedAllocation ? static_cast<const void *>(&dedicatedAllocateInfo) : static_cast<const void *>(&importMemoryInfo);
		allocateInfo.allocationSize = memoryRequirements.size;
		allocateInfo.memoryTypeIndex = memoryTypeIndex;

		RNVulkanValidate(vk::AllocateMemory(device->GetDevice(), &allocateInfo, _renderer->GetAllocatorCallback(), &_externalMemory));
		RNVulkanValidate(vk::BindImageMemory(device->GetDevice(), _image, _externalMemory, 0));

		_currentUsage = LayoutUsage::Undefined;
#else
		RN_ASSERT(false, "External Vulkan texture import is not supported on this platform");
#endif
	}

	void *VulkanTexture::GetAPITexture() const
	{
		return reinterpret_cast<void *>(_image);
	}

	void VulkanTexture::CreateImageView()
	{
		VkImageViewCreateInfo imageViewInfo = {};
		imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewInfo.pNext = nullptr;
		imageViewInfo.viewType = VulkanTextureInfo::GetImageViewType(_descriptor.type);
		imageViewInfo.format = _format;
		imageViewInfo.flags = 0;
		imageViewInfo.subresourceRange = {};
		imageViewInfo.subresourceRange.aspectMask = VulkanTextureInfo::GetAspectMask(_descriptor.format);
		imageViewInfo.subresourceRange.baseMipLevel = 0;
		imageViewInfo.subresourceRange.levelCount = _descriptor.mipMaps;
		imageViewInfo.subresourceRange.baseArrayLayer = 0;
		imageViewInfo.subresourceRange.layerCount = VulkanTextureInfo::GetImageLayerCount(_descriptor);
		imageViewInfo.image = _image;

		VulkanDevice *device = _renderer->GetVulkanDevice();
		RNVulkanValidate(vk::CreateImageView(device->GetDevice(), &imageViewInfo, _renderer->GetAllocatorCallback(), &_imageView));
	}

	VulkanTexture::SubresourceRange VulkanTexture::GetWholeSubresourceRange() const
	{
		return {
			0,
			_descriptor.mipMaps,
			0,
			VulkanTextureInfo::GetImageLayerCount(_descriptor),
			VulkanTextureInfo::GetAspectMask(_descriptor.format)
		};
	}

	bool VulkanTexture::IsWholeSubresourceRange(const SubresourceRange &range) const
	{
		const SubresourceRange wholeRange = GetWholeSubresourceRange();
		return range.baseMipmap == wholeRange.baseMipmap &&
			range.mipmapCount == wholeRange.mipmapCount &&
			range.baseLayer == wholeRange.baseLayer &&
			range.layerCount == wholeRange.layerCount &&
			range.aspectMask == wholeRange.aspectMask;
	}

	VkImageLayout VulkanTexture::GetLayoutForUsage(LayoutUsage usage) const
	{
		switch(usage)
		{
			case LayoutUsage::Undefined:
				return VK_IMAGE_LAYOUT_UNDEFINED;
			case LayoutUsage::ShaderRead:
				return VulkanTextureInfo::GetReadOnlyLayout(_descriptor.format);
			case LayoutUsage::RenderTarget:
				return VulkanTextureInfo::GetRenderTargetLayout(_descriptor.format);
			case LayoutUsage::TransferSource:
				return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			case LayoutUsage::TransferDestination:
				return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			case LayoutUsage::FragmentDensityMap:
				return VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;
		}

		RN_ASSERT(false, "Invalid Vulkan texture layout usage");
		return VK_IMAGE_LAYOUT_UNDEFINED;
	}

	VulkanTexture::BarrierIntent VulkanTexture::GetBarrierIntentForUsage(LayoutUsage usage)
	{
		switch(usage)
		{
			case LayoutUsage::Undefined:
				return BarrierIntent::ExternalSource;
			case LayoutUsage::ShaderRead:
				return BarrierIntent::ShaderSource;
			case LayoutUsage::RenderTarget:
				return BarrierIntent::RenderTarget;
			case LayoutUsage::TransferSource:
				return BarrierIntent::CopySource;
			case LayoutUsage::TransferDestination:
				return BarrierIntent::CopyDestination;
			case LayoutUsage::FragmentDensityMap:
				return BarrierIntent::ShaderSource;
		}

		RN_ASSERT(false, "Invalid Vulkan texture layout usage");
		return BarrierIntent::ShaderSource;
	}

	void VulkanTexture::TransitionToUsage(VkCommandBuffer buffer, LayoutUsage usage)
	{
		TransitionToUsage(buffer, usage, GetWholeSubresourceRange());
	}

	void VulkanTexture::TransitionToUsage(VkCommandBuffer buffer, LayoutUsage usage, const SubresourceRange &range)
	{
		const VkImageLayout sourceLayout = GetLayoutForUsage(_currentUsage);
		const VkImageLayout targetLayout = GetLayoutForUsage(usage);
		const bool isWholeImage = IsWholeSubresourceRange(range);
		if(sourceLayout == targetLayout)
		{
			if(isWholeImage)
				_currentUsage = usage;
			return;
		}

		SetImageLayout(buffer, _image, range.baseMipmap, range.mipmapCount, range.baseLayer, range.layerCount, range.aspectMask, sourceLayout, targetLayout, GetBarrierIntentForUsage(usage));

		if(isWholeImage)
		{
			_currentUsage = usage;
		}
	}

	void VulkanTexture::AdoptLayoutUsage(LayoutUsage usage)
	{
		AdoptLayoutUsage(usage, GetWholeSubresourceRange());
	}

	void VulkanTexture::AdoptLayoutUsage(LayoutUsage usage, const SubresourceRange &range)
	{
		RN_ASSERT(IsWholeSubresourceRange(range), "Partial Vulkan texture layout adoption needs per-subresource tracking");
		_currentUsage = usage;
	}

	VulkanTexture::~VulkanTexture()
	{
		StopStreamingData();

		if((_image != VK_NULL_HANDLE && !_isFromSwapchain) || _imageView != VK_NULL_HANDLE)
		{
			VkImageView imageView = _imageView;
			VkImage image = _isFromSwapchain? VK_NULL_HANDLE : _image;
			VmaAllocation allocation = _allocation;
			VkDeviceMemory externalMemory = _externalMemory;
			bool isExternalMemory = _isExternalMemory;
			VulkanRenderer *renderer = _renderer;
			renderer->AddFrameFinishedCallback([renderer, imageView, image, allocation, externalMemory, isExternalMemory]() {
				if(imageView != VK_NULL_HANDLE)
				{
					VulkanDevice *device = renderer->GetVulkanDevice();
					vk::DestroyImageView(device->GetDevice(), imageView, renderer->GetAllocatorCallback());
				}

				if(image != VK_NULL_HANDLE)
				{
					if(isExternalMemory)
					{
						VulkanDevice *device = renderer->GetVulkanDevice();
						vk::DestroyImage(device->GetDevice(), image, renderer->GetAllocatorCallback());
						if(externalMemory != VK_NULL_HANDLE)
							vk::FreeMemory(device->GetDevice(), externalMemory, renderer->GetAllocatorCallback());
					}
					else
					{
						vmaDestroyImage(renderer->_internals->memoryAllocator, image, allocation);
					}
				}
			});
		}
	}

	void VulkanTexture::StartStreamingData()
	{
		_isStreamingData = true;
	}

	void VulkanTexture::StopStreamingData()
	{
		_isStreamingData = false;

		for(StagingBuffer &buffer : _streamingUploadBuffers)
		{
			ReleaseStagingBuffer(buffer);
		}

		_streamingUploadBuffers.clear();
	}

	void VulkanTexture::SetData(uint32 mipmapLevel, const void *bytes, size_t bytesPerRow, size_t numberOfRows)
	{
		uint32 mipDepth = _descriptor.type == Texture::Type::Type3D ? std::max<uint32>(1, _descriptor.depth >> mipmapLevel) : _descriptor.depth;
		SetData(Region(0, 0, 0, _descriptor.GetWidthForMipMapLevel(mipmapLevel), _descriptor.GetHeightForMipMapLevel(mipmapLevel), mipDepth), mipmapLevel, bytes, bytesPerRow, numberOfRows);
	}

	void VulkanTexture::SetData(const Region &region, uint32 mipmapLevel, const void *bytes, size_t bytesPerRow, size_t numberOfRows)
	{
		SetData(region, mipmapLevel, 0, bytes, bytesPerRow, numberOfRows);
	}

	void VulkanTexture::SetData(const Region &region, uint32 mipmapLevel, uint32 slice, const void *bytes, size_t bytesPerRow, size_t numberOfRows)
	{
		const BufferImageCopySetup copySetup = GetBufferImageCopySetup(region, mipmapLevel, slice, bytesPerRow, numberOfRows);
		StagingBuffer uploadBuffer = {};
		StagingBuffer &activeUploadBuffer = _isStreamingData ? AcquireStreamingUploadBuffer(copySetup.size) : uploadBuffer;
		if(!_isStreamingData)
			CreateStagingBuffer(uploadBuffer, copySetup.size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

		memcpy(activeUploadBuffer.data, bytes, copySetup.size);
		RNVulkanValidate(vmaFlushAllocation(_renderer->_internals->memoryAllocator, activeUploadBuffer.allocation, 0, copySetup.size));

		LayoutUsage restoreUsage = _currentUsage;
		if(restoreUsage == LayoutUsage::Undefined)
			restoreUsage = LayoutUsage::ShaderRead;

		VulkanCommandBuffer *commandBuffer = _renderer->StartResourcesCommandBuffer();

		TransitionToUsage(commandBuffer->GetCommandBuffer(), LayoutUsage::TransferDestination);

		vk::CmdCopyBufferToImage(commandBuffer->GetCommandBuffer(), activeUploadBuffer.buffer, _image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copySetup.region);
		TransitionToUsage(commandBuffer->GetCommandBuffer(), restoreUsage);

		if(_isStreamingData)
			activeUploadBuffer.frameValue = _renderer->_currentFrame;

		_renderer->EndResourcesCommandBuffer();
		if(!_isStreamingData)
			ReleaseStagingBuffer(uploadBuffer);
	}

	VulkanTexture::BufferImageCopySetup VulkanTexture::GetBufferImageCopySetup(const Region &region, uint32 mipmapLevel, uint32 slice, size_t bytesPerRow, size_t numberOfRows) const
	{
		const VulkanTextureInfo::FormatInfo &formatInfo = VulkanTextureInfo::GetFormatInfo(_descriptor.format);
		const bool is3D = _descriptor.type == Texture::Type::Type3D;
		RN_ASSERT(region.width > 0 && region.height > 0 && region.depth > 0, "Texture upload/readback region must not be empty");
		RN_ASSERT(formatInfo.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT, "Vulkan texture upload/readback currently only supports color formats");
		RN_ASSERT(formatInfo.bytesPerBlock > 0, "Requested texture format is not supported by Vulkan (%i)", _descriptor.format);
		const size_t bytesPerBlock = std::max<size_t>(1, formatInfo.bytesPerBlock);

		const uint32 blockRows = std::max<uint32>(1, (region.height + formatInfo.blockHeight - 1) / formatInfo.blockHeight);
		const uint32 blocksPerRow = std::max<uint32>(1, (region.width + formatInfo.blockWidth - 1) / formatInfo.blockWidth);
		const size_t tightBytesPerRow = static_cast<size_t>(blocksPerRow) * bytesPerBlock;
		RN_ASSERT(bytesPerRow >= tightBytesPerRow, "Texture upload/readback bytesPerRow is too small for the requested region");
		RN_ASSERT(bytesPerRow % bytesPerBlock == 0, "Texture upload/readback bytesPerRow is not aligned to the texture format block size");
		RN_ASSERT(numberOfRows >= blockRows, "Texture upload/readback row count is too small for the requested region");

		const uint32 imageDepth = is3D ? region.depth : 1;
		const uint32 layerCount = is3D ? 1 : region.depth;
		const size_t bufferSize = bytesPerRow * numberOfRows * (is3D ? region.depth : layerCount);

		BufferImageCopySetup setup = {};
		setup.size = bufferSize;

		VkBufferImageCopy &copyRegion = setup.region;
		copyRegion.bufferOffset = 0;
		copyRegion.bufferRowLength = bytesPerRow == tightBytesPerRow ? 0 : static_cast<uint32>((bytesPerRow / bytesPerBlock) * formatInfo.blockWidth);
		copyRegion.bufferImageHeight = numberOfRows == blockRows ? 0 : static_cast<uint32>(numberOfRows * formatInfo.blockHeight);

		copyRegion.imageSubresource.aspectMask = formatInfo.aspectMask;
		copyRegion.imageSubresource.baseArrayLayer = is3D ? 0 : slice;
		copyRegion.imageSubresource.mipLevel = mipmapLevel;
		copyRegion.imageSubresource.layerCount = layerCount;

		copyRegion.imageOffset.x = region.x;
		copyRegion.imageOffset.y = region.y;
		copyRegion.imageOffset.z = is3D ? region.z : 0;

		copyRegion.imageExtent.width = region.width;
		copyRegion.imageExtent.height = region.height;
		copyRegion.imageExtent.depth = imageDepth;

		return setup;
	}

	void VulkanTexture::CreateStagingBuffer(StagingBuffer &buffer, size_t size, VkBufferUsageFlags usage, VmaAllocationCreateFlags accessFlags) const
	{
		RN_ASSERT(size > 0, "Cannot create an empty Vulkan texture staging buffer");

		buffer = {};
		buffer.size = size;
		buffer.frameValue = static_cast<size_t>(-1);

		VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bufferInfo.size = size;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VmaAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
		allocCreateInfo.flags = accessFlags | VMA_ALLOCATION_CREATE_MAPPED_BIT;

		VmaAllocationInfo allocationInfo;
		RNVulkanValidate(vmaCreateBuffer(_renderer->_internals->memoryAllocator, &bufferInfo, &allocCreateInfo, &buffer.buffer, &buffer.allocation, &allocationInfo));
		buffer.data = allocationInfo.pMappedData;
	}

	void VulkanTexture::ReleaseStagingBuffer(StagingBuffer &buffer) const
	{
		if(buffer.buffer == VK_NULL_HANDLE)
			return;

		VkBuffer stagingBuffer = buffer.buffer;
		VmaAllocation stagingAllocation = buffer.allocation;
		VulkanRenderer *renderer = _renderer;
		renderer->AddFrameFinishedCallback([renderer, stagingBuffer, stagingAllocation]() {
			vmaDestroyBuffer(renderer->_internals->memoryAllocator, stagingBuffer, stagingAllocation);
		});

		buffer.buffer = VK_NULL_HANDLE;
		buffer.allocation = VK_NULL_HANDLE;
		buffer.data = nullptr;
		buffer.size = 0;
		buffer.frameValue = static_cast<size_t>(-1);
	}

	VulkanTexture::StagingBuffer &VulkanTexture::AcquireStreamingUploadBuffer(size_t size)
	{
		const size_t unusedFrameValue = static_cast<size_t>(-1);
		for(StagingBuffer &buffer : _streamingUploadBuffers)
		{
			const bool isUnused = buffer.frameValue == unusedFrameValue;
			const bool isCompleted = _renderer->_completedFrame != unusedFrameValue && buffer.frameValue <= _renderer->_completedFrame;
			if(buffer.size >= size && (isUnused || isCompleted))
				return buffer;
		}

		StagingBuffer buffer = {};
		CreateStagingBuffer(buffer, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
		_streamingUploadBuffers.push_back(buffer);
		return _streamingUploadBuffers.back();
	}

	void VulkanTexture::GetData(void *bytes, uint32 mipmapLevel, size_t bytesPerRow, std::function<void(void)> callback) const
	{
		//TODO: Force main thread, or make it more flexible

		VulkanTexture *texture = const_cast<VulkanTexture *>(this);
		LayoutUsage restoreUsage = _currentUsage;
		if(restoreUsage == LayoutUsage::Undefined)
			restoreUsage = LayoutUsage::ShaderRead;
		const uint32 mipWidth = _descriptor.GetWidthForMipMapLevel(mipmapLevel);
		const uint32 mipHeight = _descriptor.GetHeightForMipMapLevel(mipmapLevel);
		const uint32 mipDepth = _descriptor.type == Texture::Type::Type3D ? std::max<uint32>(1, _descriptor.depth >> mipmapLevel) : VulkanTextureInfo::GetImageLayerCount(_descriptor);

		const VulkanTextureInfo::FormatInfo &formatInfo = VulkanTextureInfo::GetFormatInfo(_descriptor.format);
		const uint32 blockRows = std::max<uint32>(1, (mipHeight + formatInfo.blockHeight - 1) / formatInfo.blockHeight);
		const BufferImageCopySetup copySetup = GetBufferImageCopySetup(Region(0, 0, 0, mipWidth, mipHeight, mipDepth), mipmapLevel, 0, bytesPerRow, blockRows);

		StagingBuffer downloadBuffer = {};
		CreateStagingBuffer(downloadBuffer, copySetup.size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);

		VulkanCommandBuffer *commandBuffer = _renderer->GetCommandBuffer();
		commandBuffer->Begin();
		texture->TransitionToUsage(commandBuffer->GetCommandBuffer(), LayoutUsage::TransferSource);
		vk::CmdCopyImageToBuffer(commandBuffer->GetCommandBuffer(), _image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, downloadBuffer.buffer, 1, &copySetup.region);
		texture->TransitionToUsage(commandBuffer->GetCommandBuffer(), restoreUsage);

		commandBuffer->End();
		_renderer->SubmitCommandBuffer(commandBuffer);

		_renderer->AddFrameFinishedCallback([this, downloadBuffer, callback, bytes]() {
			RNVulkanValidate(vmaInvalidateAllocation(_renderer->_internals->memoryAllocator, downloadBuffer.allocation, 0, downloadBuffer.size));
			memcpy(bytes, downloadBuffer.data, downloadBuffer.size);
			vmaDestroyBuffer(_renderer->_internals->memoryAllocator, downloadBuffer.buffer, downloadBuffer.allocation);
			callback();
		});
	}

	void VulkanTexture::GenerateMipMaps()
	{
		_renderer->CreateMipMapForTexture(this);
	}

	void VulkanTexture::GenerateMipMaps(VkCommandBuffer commandBuffer)
	{
		if(_descriptor.mipMaps <= 1)
			return;

		const VkImageAspectFlags aspectMask = VulkanTextureInfo::GetAspectMask(_descriptor.format);
		const uint32 imageLayers = VulkanTextureInfo::GetImageLayerCount(_descriptor);
		const VkImageLayout sourceLayout = GetLayoutForUsage(_currentUsage);
		RN_ASSERT(aspectMask == VK_IMAGE_ASPECT_COLOR_BIT, "Vulkan mipmap generation currently only supports color textures");

		//TODO: Fix mipmap generation for texture arrays
		SetImageLayout(commandBuffer, _image, 0, 1, 0, 1, aspectMask, sourceLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, BarrierIntent::CopySource);
		SetImageLayout(commandBuffer, _image, 1, _descriptor.mipMaps-1, 0, 1, aspectMask, sourceLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, BarrierIntent::CopyDestination);
		for(uint16 i = 0; i < _descriptor.mipMaps-1; i++)
		{
			VkImageBlit imageBlit = {};

			imageBlit.srcSubresource.aspectMask = aspectMask;
			imageBlit.srcSubresource.mipLevel = i;
			imageBlit.srcSubresource.baseArrayLayer = 0;
			imageBlit.srcSubresource.layerCount = 1;

			imageBlit.srcOffsets[0].x = 0;
			imageBlit.srcOffsets[0].y = 0;
			imageBlit.srcOffsets[0].z = 0;
			imageBlit.srcOffsets[1].x = _descriptor.GetWidthForMipMapLevel(i);
			imageBlit.srcOffsets[1].y = _descriptor.GetHeightForMipMapLevel(i);
			imageBlit.srcOffsets[1].z = 1;

			imageBlit.dstSubresource.aspectMask = aspectMask;
			imageBlit.dstSubresource.mipLevel = i+1;
			imageBlit.dstSubresource.baseArrayLayer = 0;
			imageBlit.dstSubresource.layerCount = 1;

			imageBlit.dstOffsets[0].x = 0;
			imageBlit.dstOffsets[0].y = 0;
			imageBlit.dstOffsets[0].z = 0;
			imageBlit.dstOffsets[1].x = _descriptor.GetWidthForMipMapLevel(i+1);
			imageBlit.dstOffsets[1].y = _descriptor.GetHeightForMipMapLevel(i+1);
			imageBlit.dstOffsets[1].z = 1;

			vk::CmdBlitImage(commandBuffer, _image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, _image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlit, VK_FILTER_LINEAR);

			SetImageLayout(commandBuffer, _image, i + 1, 1, 0, 1, aspectMask, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, BarrierIntent::CopySource);
		}

		SetImageLayout(commandBuffer, _image, 0, _descriptor.mipMaps, 0, imageLayers, aspectMask, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, BarrierIntent::ShaderSource);
		AdoptLayoutUsage(LayoutUsage::ShaderRead);
	}

	void VulkanTexture::SetImageLayout(VkCommandBuffer buffer, VkImage image, uint32 baseMipmap, uint32 mipmapCount, uint32 baseLayer, uint32 layerCount, VkImageAspectFlags aspectMask, VkImageLayout fromLayout, VkImageLayout toLayout, BarrierIntent intent)
	{
		VkImageMemoryBarrier imageMemoryBarrier = {};
		imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageMemoryBarrier.pNext = nullptr;
		imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageMemoryBarrier.oldLayout = fromLayout;
		imageMemoryBarrier.newLayout = toLayout;
		imageMemoryBarrier.image = image;
		imageMemoryBarrier.subresourceRange.aspectMask = aspectMask;
		imageMemoryBarrier.subresourceRange.baseMipLevel = baseMipmap;
		imageMemoryBarrier.subresourceRange.levelCount = mipmapCount;
		imageMemoryBarrier.subresourceRange.baseArrayLayer = baseLayer;
		imageMemoryBarrier.subresourceRange.layerCount = layerCount;

		imageMemoryBarrier.srcAccessMask = 0;
		imageMemoryBarrier.dstAccessMask = 0;

		VkPipelineStageFlags srcStageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		VkPipelineStageFlags destStageFlags = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

		if(intent == BarrierIntent::ShaderSource)
		{
			srcStageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
			// Textures may be read in multiple shader stages; include both common graphics stages
			destStageFlags = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}

		if(intent == BarrierIntent::CopySource)
		{
			srcStageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destStageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;

			if(fromLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) srcStageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}

		if(intent == BarrierIntent::CopyDestination)
		{
			srcStageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			destStageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}

		if(intent == BarrierIntent::RenderTarget)
		{
			srcStageFlags = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			destStageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

			if(fromLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
			{
				srcStageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
			}

			if(toLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) destStageFlags = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		}

		if(intent == BarrierIntent::ExternalSource)
		{
			srcStageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			destStageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}

		// Ensure src stage covers src access implied by old layout
		if(fromLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		{
			srcStageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		}
		else if(fromLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
		{
			srcStageFlags = static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
		}
		else if(fromLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			// Cover common shader stages that read
			srcStageFlags = static_cast<VkPipelineStageFlags>(srcStageFlags | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);
		}
		else if(fromLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL || fromLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
		{
			srcStageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}

		// Source layouts (old)

		// Old layout is color attachment
		// Make sure any writes to the color buffer have been finished
		if(fromLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		// Old layout is shader read (sampler, input attachment)
		// Make sure any shader reads from the image have been finished
		if(fromLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;

		if(fromLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		// Target layouts (new)

		// New layout is transfer destination (copy, blit)
		// Make sure any copyies to the image have been finished
		if(toLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		// New layout is transfer source (copy, blit)
		// Make sure any reads from and writes to the image have been finished
		if(toLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
		{
			//imageMemoryBarrier.srcAccessMask |= VK_ACCESS_TRANSFER_READ_BIT;
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		}

		// New layout is color attachment
		// Make sure any writes to the color buffer hav been finished
		if(toLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		{
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			//imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT; //Used to be here
			//destStageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		}

		if(toLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
		{
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		}

		// New layout is depth attachment
		// Make sure any writes to depth/stencil buffer have been finished
		if(toLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
			imageMemoryBarrier.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		// New layout is shader read (sampler, input attachment)
		// Ensure subsequent shader reads are synchronized
		if(toLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		}

		if(fromLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		// Undefined layout
		// Only allowed as initial layout!
		// Make sure any writes to the image have been finished
		if(fromLayout == VK_IMAGE_LAYOUT_UNDEFINED) //TODO: Check if there is a case where this is needed...
			imageMemoryBarrier.srcAccessMask = 0;// VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;

		// Put barrier inside setup command buffer
		vk::CmdPipelineBarrier(buffer, srcStageFlags, destStageFlags, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
	}
}
