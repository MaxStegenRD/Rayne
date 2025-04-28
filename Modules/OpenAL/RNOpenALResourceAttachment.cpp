//
//  RNOpenALResourceAttachment.cpp
//  Rayne-OpenAL
//
//  Copyright 2017 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNOpenALResourceAttachment.h"

#include "RNOpenALWorld.h"

#include <AL/al.h>
#include <AL/alc.h>

static const char *kResourceAttachmentKey = "kResourceAttachmentKey";

namespace RN
{
	OpenALResourceAttachment::OpenALResourceAttachment(RN::AudioAsset *resource) : _resource(resource)
	{
		
	}

	OpenALResourceAttachment::~OpenALResourceAttachment()
	{
		for(auto pair : _bufferID)
		{
			pair.first->MakeCurrent(); //This could crash if a device was deleted before this attachment!
			alDeleteBuffers(1, &pair.second);
		}
	}

	uint32 OpenALResourceAttachment::GetBufferID(OpenALOutputDevice *outputDevice)
	{
		if(_bufferID.count(outputDevice) > 0)
		{
			return _bufferID[outputDevice];
		}
		
		outputDevice->MakeCurrent();
		uint32 bufferID;
		alGenBuffers(1, &bufferID);
		alBufferData(bufferID, GetALFormat(_resource->GetChannels(), _resource->GetBytesPerSample() * 8), _resource->GetData()->GetBytes(), static_cast<int>(_resource->GetData()->GetLength()), _resource->GetSampleRate());
		
		_bufferID[outputDevice] = bufferID;
		
		return bufferID;
	}

	int OpenALResourceAttachment::GetALFormat(short channels, short bitsPerSample)
	{
		bool stereo = (channels > 1);

		switch(bitsPerSample / channels)
		{
			case 16:
				if(stereo)
					return AL_FORMAT_STEREO16;
				else
					return AL_FORMAT_MONO16;
			case 8:
				if(stereo)
					return AL_FORMAT_STEREO8;
				else
					return AL_FORMAT_MONO8;
			default:
				return -1;
		}
	}

	OpenALResourceAttachment *OpenALResourceAttachment::GetAttachmentForResource(RN::AudioAsset *resource)
	{
		RN::Object *object = resource->GetAssociatedObject(kResourceAttachmentKey);
		if(object)
		{
			OpenALResourceAttachment *resourceAttachment = object->Downcast<OpenALResourceAttachment>();
			RN_ASSERT(resourceAttachment, "Audio Resource attachment must be of Type AudioResourceAttachment");
			return resourceAttachment;
		}

		OpenALResourceAttachment *resourceAttachment = new OpenALResourceAttachment(resource);
		resource->SetAssociatedObject(kResourceAttachmentKey, resourceAttachment, RN::Object::MemoryPolicy::Retain);
		resourceAttachment->Release();

		return resourceAttachment;
	}
} // namespace RN
