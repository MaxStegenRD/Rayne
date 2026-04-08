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
	constexpr size_t kRNApplicationLogArchiveCount = 2;

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

	String *Application::GetArchivedLogFilePath(const String *path, size_t index) const
	{
		String *directoryPath = path->StringByDeletingLastPathComponent();
		String *baseName = path->GetLastPathComponent();
		String *baseFileName = baseName->StringByDeletingPathExtension();
		String *extension = baseName->GetPathExtension();
		String *archiveFileName = extension ? RNSTRF("%s.%llu.%s", baseFileName->GetUTF8String(), static_cast<unsigned long long>(index), extension->GetUTF8String()) : RNSTRF("%s.%llu", baseFileName->GetUTF8String(), static_cast<unsigned long long>(index));
		return directoryPath->StringByAppendingPathComponent(archiveFileName);
	}

	void Application::RotateLogFiles(const String *path) const
	{
		if(!FileManager::PathExists(path))
			return;

		FileManager *fileManager = FileManager::GetSharedInstance();
		String *oldestArchivePath = GetArchivedLogFilePath(path, kRNApplicationLogArchiveCount);
		if(FileManager::PathExists(oldestArchivePath))
			fileManager->DeleteFile(oldestArchivePath);

		for(size_t index = kRNApplicationLogArchiveCount; index > 1; index--)
		{
			String *sourcePath = GetArchivedLogFilePath(path, index - 1);
			if(FileManager::PathExists(sourcePath))
			{
				String *destinationPath = GetArchivedLogFilePath(path, index);
				fileManager->RenameFile(sourcePath, destinationPath);
			}
		}

		String *firstArchivePath = GetArchivedLogFilePath(path, 1);
		fileManager->RenameFile(path, firstArchivePath);
	}

	Array *Application::GetLoggingEngines()
	{
		DebugLogFormatter *formatter = new DebugLogFormatter();

#if RN_PLATFORM_WINDOWS
	#if RN_BUILD_DEBUG
		LoggingEngine *engine = new WideCharStreamLoggingEngine(nullptr, true);
	#else
		String *logFilePath = GetDefaultLogFilePath();
		RotateLogFiles(logFilePath);
		LoggingEngine *engine = new WideCharStreamLoggingEngine(logFilePath->GetUTF8String(), true);
	#endif
		engine->SetLogFormatter(formatter->Autorelease());
		return Array::WithObjects({engine->Autorelease()});
#else
		LoggingEngine *engine = new StreamLoggingEngine(nullptr, true);
		engine->SetLogFormatter(formatter->Autorelease());

		String *logFilePath = GetDefaultLogFilePath();
		RotateLogFiles(logFilePath);
		LoggingEngine *engine2 = new StreamLoggingEngine(logFilePath->GetUTF8String(), true);
		engine2->SetLogFormatter(formatter);

		return Array::WithObjects({engine->Autorelease(), engine2->Autorelease()});
#endif
	}
} // namespace RN
