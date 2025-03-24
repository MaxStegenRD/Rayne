//
//  RNApplication.h
//  Rayne
//
//  Copyright 2015 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_APPLICATION_H__
#define __RAYNE_APPLICATION_H__

#include "../Objects/RNString.h"
#include "../Rendering/RNRenderer.h"
#include "RNBase.h"

namespace RN
{
	class Application
	{
	public:
		friend class Kernel;

		RNAPI virtual ~Application();

		RNAPI virtual void WillFinishLaunching(Kernel *kernel);
		RNAPI virtual void DidFinishLaunching(Kernel *kernel);

		RNAPI virtual void WillExit();

		RNAPI virtual void WillStep(float delta);
		RNAPI virtual void DidStep(float delta);

		RNAPI virtual void WillBecomeActive();
		RNAPI virtual void DidBecomeActive();
		RNAPI virtual void WillResignActive();
		RNAPI virtual void DidResignActive();

		RNAPI virtual RendererDescriptor *GetPreferredRenderer() const;
		RNAPI virtual RenderingDevice *GetPreferredRenderingDevice(RN::RendererDescriptor *descriptor, const Array *devices) const;

		RNAPI virtual uint64 GetBuildNumber() const { return 0; }

		RNAPI Array *GetLoggingEngines();

		const String *GetTitle() const { return _title; }

	protected:
		RNAPI Application();

	private:
		void __PrepareForWillFinishLaunching(Kernel *kernel);
#if RN_PLATFORM_WINDOWS
		std::wofstream _fileStream;
#else
		std::ofstream _fileStream;
#endif

		String *_title;
	};
} // namespace RN

#endif /* __RAYNE_APPLICATION_H__ */
