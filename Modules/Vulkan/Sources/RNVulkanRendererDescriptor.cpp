//
//  RNVulkanRendererDescriptor.cpp
//  Rayne
//
//  Copyright 2016 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNVulkanRendererDescriptor.h"
#include "RNVulkanDispatchTable.h"
#include "RNVulkanGraphicsProvider.h"
#include "RNVulkanRenderer.h"
#include "RNVulkanDevice.h"

namespace RN
{
	RNDefineMeta(VulkanRendererDescriptor, RendererDescriptor)

	void VulkanRendererDescriptor::InitialWakeUp(MetaClass *meta)
	{
		if(meta == VulkanRendererDescriptor::GetMetaClass())
		{
			VulkanRendererDescriptor *descriptor = new VulkanRendererDescriptor();
			GetExtensionPoint()->AddExtension(descriptor, 0);
			descriptor->Release();
		}
	}

	VulkanRendererDescriptor::VulkanRendererDescriptor() :
		RN::RendererDescriptor(RNCSTR("net.uberpixel.rendering.vulkan"), RNCSTR("Vulkan")),
		_instance(nullptr),
		_graphicsProvider(nullptr)
	{}

	VulkanRendererDescriptor::~VulkanRendererDescriptor()
	{
		SafeRelease(_graphicsProvider);
	}

	Renderer *VulkanRendererDescriptor::CreateRenderer(RenderingDevice *tdevice)
	{
		VulkanDevice *device = static_cast<VulkanDevice *>(tdevice);
		if(device->CreateDevice(_instance->GetDeviceExtensions(), _graphicsProvider))
		{
			SafeRelease(_graphicsProvider);

			vk::init_dispatch_table_bottom(_instance->GetInstance(), device->GetDevice());

			VulkanRenderer *renderer = new VulkanRenderer(this, device);
			return renderer;
		}

		return nullptr;
	}

	void VulkanRendererDescriptor::PrepareWithSettings(const Dictionary *settings)
	{
		Array *instanceExtensions = nullptr;
		Array *deviceExtensions = nullptr;
		VulkanGraphicsProvider *graphicsProvider = nullptr;
		if(settings)
		{
			instanceExtensions = settings->GetObjectForKey<Array>(RNCSTR("instanceextensions"));
			deviceExtensions = settings->GetObjectForKey<Array>(RNCSTR("deviceextensions"));
			graphicsProvider = settings->GetObjectForKey<VulkanGraphicsProvider>(RNCSTR("vulkangraphicsprovider"));
		}
		_graphicsProvider = graphicsProvider;
		SafeRetain(_graphicsProvider);
		
		_instance = new VulkanInstance(instanceExtensions, deviceExtensions);

		if(!_instance->LoadVulkan(_graphicsProvider))
		{
			SafeRelease(_graphicsProvider);
			delete _instance;
			_instance = nullptr;
		}
	}

	bool VulkanRendererDescriptor::CanCreateRenderer() const
	{
		return (_instance && _instance->GetDevices()->GetCount() > 0);
	}
}
