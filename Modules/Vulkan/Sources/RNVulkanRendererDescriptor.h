//
//  RNVulkanRendererDescriptor.h
//  Rayne
//
//  Copyright 2016 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_VULKANRENDERERDESCRIPTOR_H_
#define __RAYNE_VULKANRENDERERDESCRIPTOR_H_

#include "RNVulkan.h"
#include "RNVulkanInstance.h"

namespace RN
{
	class VulkanGraphicsProvider;
	class VulkanRendererDescriptor : public RendererDescriptor
	{
	public:
		VKAPI static void InitialWakeUp(MetaClass *meta);

		VKAPI ~VulkanRendererDescriptor() override;

		VKAPI Renderer *CreateRenderer(RenderingDevice *device) override;
		VKAPI bool CanCreateRenderer() const override;

		const Array *GetDevices() const override { return _instance->GetDevices(); }
		VKAPI void PrepareWithSettings(const Dictionary *settings) override;

		VulkanInstance *GetInstance() const { return _instance; }

	private:
		VulkanRendererDescriptor();

		VulkanInstance *_instance;
		VulkanGraphicsProvider *_graphicsProvider;

		RNDeclareMetaAPI(VulkanRendererDescriptor, VKAPI)
	};
}


#endif /* __RAYNE_VULKANRENDERERDESCRIPTOR_H_ */
