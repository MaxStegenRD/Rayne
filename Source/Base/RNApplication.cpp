//
//  RNApplication.cpp
//  Rayne
//
//  Copyright 2015 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNApplication.h"
#include "../Debug/RNLoggingEngine.h"
#include "../Rendering/RNRenderingDevice.h"
#include "RNKernel.h"

namespace RN
{
	Application::Application() :
		_title(nullptr)
	{
	}

	Application::~Application()
	{
		SafeRelease(_title);
	}

	void Application::__PrepareForWillFinishLaunching(Kernel *kernel)
	{
		_title = kernel->GetManifestEntryForKey<String>(kRNManifestApplicationKey);
		_title->Retain();
	}

	void Application::WillFinishLaunching(Kernel *kernel)
	{}
	void Application::DidFinishLaunching(Kernel *kernel)
	{}
	void Application::WillExit()
	{}

	void Application::WillStep(float delta)
	{}
	void Application::DidUpdate(float delta)
	{}
	void Application::DidStep(float delta)
	{}

	void Application::WillBecomeActive()
	{}
	void Application::DidBecomeActive()
	{}
	void Application::WillResignActive()
	{}
	void Application::DidResignActive()
	{}

	RendererDescriptor *Application::GetPreferredRenderer() const
	{
		return nullptr;
	}

	RenderingDevice *Application::GetPreferredRenderingDevice(RN::RendererDescriptor *descriptor, const Array *devices) const
	{
		return devices->GetFirstObject<RenderingDevice>();
	}

	String *Application::GetDefaultLogFilePath() const
	{
		String *loggingFilePath = FileManager::GetSharedInstance()->GetPathForLocation(FileManager::Location::ExternalSaveDirectory);
		loggingFilePath->AppendPathComponent(RNCSTR("logs"));
		FileManager::GetSharedInstance()->CreateDirectory(loggingFilePath); //Only does something if the directories don`t exist yet!
		loggingFilePath->AppendPathComponent(RNCSTR("RNLogs.txt"));
		return loggingFilePath;
	}

	Array *Application::GetLoggingEngines()
	{
		DebugLogFormatter *formatter = new DebugLogFormatter();

#if RN_PLATFORM_WINDOWS
	#if RN_BUILD_DEBUG
		LoggingEngine *engine = new WideCharStreamLoggingEngine(nullptr, true);
	#else
		LoggingEngine *engine = new WideCharStreamLoggingEngine(GetDefaultLogFilePath()->GetUTF8String(), true);
	#endif
		engine->SetLogFormatter(formatter->Autorelease());
		return Array::WithObjects({engine->Autorelease()});
#else
		LoggingEngine *engine = new StreamLoggingEngine(nullptr, true);
		engine->SetLogFormatter(formatter->Autorelease());

		LoggingEngine *engine2 = new StreamLoggingEngine(GetDefaultLogFilePath()->GetUTF8String(), true);
		engine2->SetLogFormatter(formatter);

		return Array::WithObjects({engine->Autorelease(), engine2->Autorelease()});
#endif
	}
} // namespace RN
