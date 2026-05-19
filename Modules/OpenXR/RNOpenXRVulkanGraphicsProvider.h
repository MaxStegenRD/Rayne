//
//  RNOpenXRVulkanGraphicsProvider.h
//  Rayne-OpenXR
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_OpenXRVULKANGRAPHICSPROVIDER_H_
#define __RAYNE_OpenXRVULKANGRAPHICSPROVIDER_H_

#include "RNOpenXR.h"
#include "RNVulkanGraphicsProvider.h"

#include "openxr/openxr.h"
#include "openxr/openxr_platform.h"

namespace RN
{
	class OpenXRVulkanGraphicsProvider : public VulkanGraphicsProvider
	{
	public:
		OpenXRVulkanGraphicsProvider(XrInstance instance, XrSystemId systemID, PFN_xrCreateVulkanInstanceKHR createVulkanInstance, PFN_xrCreateVulkanDeviceKHR createVulkanDevice);

		VkResult CreateVulkanInstance(PFN_vkGetInstanceProcAddr getInstanceProcAddr, const VkInstanceCreateInfo *createInfo, const VkAllocationCallbacks *allocator, VkInstance *instance) final;
		VkResult CreateVulkanDevice(PFN_vkGetInstanceProcAddr getInstanceProcAddr, VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *createInfo, const VkAllocationCallbacks *allocator, VkDevice *device) final;

	private:
		XrInstance _instance;
		XrSystemId _systemID;
		PFN_xrCreateVulkanInstanceKHR _createVulkanInstance;
		PFN_xrCreateVulkanDeviceKHR _createVulkanDevice;

		RNDeclareMeta(OpenXRVulkanGraphicsProvider)
	};
}

#endif /* __RAYNE_OpenXRVULKANGRAPHICSPROVIDER_H_ */
