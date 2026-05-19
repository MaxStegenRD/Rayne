//
//  RNVulkanGraphicsProvider.h
//  Rayne
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_VULKANGRAPHICSPROVIDER_H_
#define __RAYNE_VULKANGRAPHICSPROVIDER_H_

#include "RNVulkan.h"

namespace RN
{
	class VulkanGraphicsProvider : public Object
	{
	public:
		VKAPI virtual VkResult CreateVulkanInstance(PFN_vkGetInstanceProcAddr getInstanceProcAddr, const VkInstanceCreateInfo *createInfo, const VkAllocationCallbacks *allocator, VkInstance *instance) = 0;
		VKAPI virtual VkResult CreateVulkanDevice(PFN_vkGetInstanceProcAddr getInstanceProcAddr, VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *createInfo, const VkAllocationCallbacks *allocator, VkDevice *device) = 0;

		RNDeclareMetaAPI(VulkanGraphicsProvider, VKAPI)

	protected:
		VulkanGraphicsProvider() = default;
		~VulkanGraphicsProvider() override = default;
	};

}

#endif /* __RAYNE_VULKANGRAPHICSPROVIDER_H_ */
