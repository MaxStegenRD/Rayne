//
//  RNOpenXRVulkanGraphicsProvider.cpp
//  Rayne-OpenXR
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNOpenXR.h"

#if XR_USE_GRAPHICS_API_VULKAN
	#include "RNOpenXRVulkanGraphicsProvider.h"

namespace RN
{
	RNDefineMeta(OpenXRVulkanGraphicsProvider, VulkanGraphicsProvider)

	OpenXRVulkanGraphicsProvider::OpenXRVulkanGraphicsProvider(XrInstance instance, XrSystemId systemID, PFN_xrCreateVulkanInstanceKHR createVulkanInstance, PFN_xrCreateVulkanDeviceKHR createVulkanDevice) :
		_instance(instance),
		_systemID(systemID),
		_createVulkanInstance(createVulkanInstance),
		_createVulkanDevice(createVulkanDevice)
	{}

	VkResult OpenXRVulkanGraphicsProvider::CreateVulkanInstance(PFN_vkGetInstanceProcAddr getInstanceProcAddr, const VkInstanceCreateInfo *createInfo, const VkAllocationCallbacks *allocator, VkInstance *instance)
	{
		if(!_createVulkanInstance) return VK_ERROR_INITIALIZATION_FAILED;

		XrVulkanInstanceCreateInfoKHR vulkanInstanceCreateInfo;
		vulkanInstanceCreateInfo.type = XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR;
		vulkanInstanceCreateInfo.next = nullptr;
		vulkanInstanceCreateInfo.systemId = _systemID;
		vulkanInstanceCreateInfo.createFlags = 0;
		vulkanInstanceCreateInfo.pfnGetInstanceProcAddr = getInstanceProcAddr;
		vulkanInstanceCreateInfo.vulkanCreateInfo = createInfo;
		vulkanInstanceCreateInfo.vulkanAllocator = allocator;

		VkResult vulkanResult = VK_SUCCESS;
		XrResult result = _createVulkanInstance(_instance, &vulkanInstanceCreateInfo, instance, &vulkanResult);
		if(XR_FAILED(result))
		{
			char resultString[XR_MAX_RESULT_STRING_SIZE] = {};
			xrResultToString(_instance, result, resultString);
			RNError("OpenXR Vulkan instance creation failed: " << result << " (" << resultString << ")");
			return VK_ERROR_INITIALIZATION_FAILED;
		}

		return vulkanResult;
	}

	VkResult OpenXRVulkanGraphicsProvider::CreateVulkanDevice(PFN_vkGetInstanceProcAddr getInstanceProcAddr, VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *createInfo, const VkAllocationCallbacks *allocator, VkDevice *device)
	{
		if(!_createVulkanDevice) return VK_ERROR_INITIALIZATION_FAILED;

		XrVulkanDeviceCreateInfoKHR vulkanDeviceCreateInfo;
		vulkanDeviceCreateInfo.type = XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR;
		vulkanDeviceCreateInfo.next = nullptr;
		vulkanDeviceCreateInfo.systemId = _systemID;
		vulkanDeviceCreateInfo.createFlags = 0;
		vulkanDeviceCreateInfo.pfnGetInstanceProcAddr = getInstanceProcAddr;
		vulkanDeviceCreateInfo.vulkanPhysicalDevice = physicalDevice;
		vulkanDeviceCreateInfo.vulkanCreateInfo = createInfo;
		vulkanDeviceCreateInfo.vulkanAllocator = allocator;

		VkResult vulkanResult = VK_SUCCESS;
		XrResult result = _createVulkanDevice(_instance, &vulkanDeviceCreateInfo, device, &vulkanResult);
		if(XR_FAILED(result))
		{
			char resultString[XR_MAX_RESULT_STRING_SIZE] = {};
			xrResultToString(_instance, result, resultString);
			RNError("OpenXR Vulkan device creation failed: " << result << " (" << resultString << ")");
			return VK_ERROR_INITIALIZATION_FAILED;
		}

		return vulkanResult;
	}
}
#endif
