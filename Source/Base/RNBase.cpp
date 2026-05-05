//
//  RNBase.cpp
//  Rayne
//
//  Copyright 2015 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNBaseInternal.h"
#include "RNAndroidState.h"
#include "RNKernel.h"

#if RN_PLATFORM_WINDOWS
	#include <fcntl.h>
	#include <io.h>
#else
	#include <locale>
#endif

#if RN_PLATFORM_ANDROID
	#include <android/log.h>
	#include <sys/prctl.h>
#endif

#if RN_PLATFORM_MAC_OS

@interface RNApplication : NSApplication<NSApplicationDelegate>
@end

@implementation RNApplication
{
	RN::Kernel *_kernel;
}

- (void)setKernel:(RN::Kernel *)kernel
{
	_kernel = kernel;
}

- (void)sendEvent:(NSEvent *)event
{
	if([event type] == NSEventTypeKeyUp && ([event modifierFlags] & NSEventModifierFlagCommand))
	{
		[[self keyWindow] sendEvent:event];
	}

	if([event type] == NSEventTypeKeyDown || [event type] == NSEventTypeKeyUp)
	{
		RN::InputManager *inputManager = RN::InputManager::GetSharedInstance();
		if(inputManager) inputManager->ProcessKeyEvent([event keyCode], [event type] == NSEventTypeKeyDown);
	}

	if([event type] == NSEventTypeFlagsChanged)
	{
		//https://developer.apple.com/documentation/appkit/nseventmodifierflags?language=objc
		bool isPressed = false;
		uint16 keyCode = [event keyCode];
		if((keyCode == 56 || keyCode == 60))
		{
			keyCode = 56;
			isPressed = (event.modifierFlags & NSEventModifierFlagShift);
		}

		RN::InputManager *inputManager = RN::InputManager::GetSharedInstance();
		if(inputManager) inputManager->ProcessKeyEvent(keyCode, isPressed);
	}

	[super sendEvent:event];
}

- (void)applicationWillBecomeActive:(NSNotification *)notification
{
	_kernel->__WillBecomeActive();
}

- (void)applicationDidBecomeActive:(NSNotification *)notification
{
	_kernel->__DidBecomeActive();
}

- (void)applicationWillResignActive:(NSNotification *)notification
{
	_kernel->__WillResignActive();
}
- (void)applicationDidResignActive:(NSNotification *)notification
{
	_kernel->__DidResignActive();
}

@end

#endif

#if RN_PLATFORM_ANDROID
static void Android_enqueue_cmd(android_app *app, int32_t cmd)
{
	RN::AndroidState *androidState = static_cast<RN::AndroidState *>(app->userData);
	if(!androidState)
		return;

	androidState->HandleTransientCommand(cmd);
}
#endif

#if RN_PLATFORM_ANDROID
// Helpder class to forward the cout/cerr output to logcat derived from:
// http://stackoverflow.com/questions/8870174/is-stdcout-usable-in-android-ndk
class AndroidBuffer : public std::streambuf
{
public:
	AndroidBuffer(android_LogPriority priority)
	{
		priority_ = priority;
		this->setp(buffer_, buffer_ + kBufferSize - 1);
	}

private:
	static const size_t kBufferSize = 1024;
	int32_t overflow(int32_t c)
	{
		if(c == traits_type::eof())
		{
			*this->pptr() = traits_type::to_char_type(c);
			this->sbumpc();
		}
		return this->sync() < 0 ? traits_type::eof() : traits_type::not_eof(c);
	}

	int32_t sync()
	{
		int32_t rc = 0;
		if(this->pbase() != this->pptr())
		{
			size_t sizeToWrite = this->pptr() - this->pbase();
			char writebuf[kBufferSize + 1];
			memcpy(writebuf, this->pbase(), sizeToWrite); //Truncate logs if longer than the size of the buffer
			writebuf[sizeToWrite] = '\0';

			rc = __android_log_write(priority_, "std", writebuf) > 0 ? 0 : -1;
			this->setp(buffer_, buffer_ + kBufferSize - 1);
		}
		return rc;
	}

	android_LogPriority priority_ = ANDROID_LOG_INFO;
	char buffer_[kBufferSize];
};
#endif

namespace RN
{
#if RN_ENABLE_VTUNE
	__itt_domain *VTuneDomain;
#endif

	struct __KernelBootstrapHelper
	{
	public:
		static Kernel *BootstrapKernel(Application *app, const ArgumentParser &arguments, void *object
#if RN_PLATFORM_ANDROID
									   , AndroidState *androidState
#endif
		)
		{
#if RN_PLATFORM_WINDOWS
			if(!arguments.HasArgument("no-locale", '\0'))
			{
				_setmode(_fileno(stdout), _O_U16TEXT);
			}
#else
			if(!arguments.HasArgument("no-locale", '\0'))
			{
				const char *locale = "en_US.UTF-8";

				try
				{
					ArgumentParser::Argument argument = arguments.ParseArgument("locale", 'l');
					locale = argument.GetValue()->GetUTF8String();
				}
				catch(ArgumentNotFoundException &)
				{}

				const char *result = setlocale(LC_ALL, locale);
				if(!result)
					std::cerr << "Couldn't set locale " << locale << std::endl;
			}
#endif

			Kernel *result =
#if RN_PLATFORM_ANDROID
				new Kernel(app, arguments, androidState);
#else
				new Kernel(app, arguments);
#endif

#if RN_PLATFORM_ANDROID
			android_app *androidApp = static_cast<android_app *>(object);
			RN_ASSERT(androidApp, "Object needs to be a pointer to the android_app object for Android builds.");
			RN_ASSERT(androidState, "AndroidState must be available before bootstrapping on Android.");

			JNIEnv *env = nullptr;
			androidApp->activity->vm->AttachCurrentThread(&env, nullptr);

			// Note that AttachCurrentThread will reset the thread name.
			prctl(PR_SET_NAME, (long)"RN::Main", 0, 0, 0);

			result->InitializeAndroid(env);
#endif

#if RN_PLATFORM_MAC_OS
			@autoreleasepool
			{
				result->Bootstrap();
			}
#elif RN_PLATFORM_IOS
			RN_ASSERT(object, "Object needs to be a pointer to a CAMetalLayer for iOS builds.");

			@autoreleasepool
			{
				result->SetMetalLayer(object);
				result->Bootstrap();
			}
#elif RN_PLATFORM_VISIONOS
			RN_ASSERT(object, "Object needs to be a pointer to a Layer Renderer for visionOS builds.");

			@autoreleasepool
			{
				result->SetLayerRenderer(object);
				result->Bootstrap();
			}
#else
			result->Bootstrap();
#endif

			return result;
		}

#if RN_PLATFORM_ANDROID
		static AndroidState *CreateAndroidState(android_app *androidApp)
		{
			AndroidState *androidState = new AndroidState();
			androidState->SetApp(androidApp);
			androidApp->onAppCmd = Android_enqueue_cmd;

			return androidState;
		}

		static void ProcessAndroidEvents(AndroidState *androidState, int timeout)
		{
			int ident;
			int events;
			android_poll_source *source = nullptr;
			bool shouldWait = true;

			do
			{
				ident = ALooper_pollOnce(shouldWait? timeout : 0, nullptr, &events, reinterpret_cast<void **>(&source));
				shouldWait = false;

				if(ident >= 0 && source)
				{
					androidState->ProcessSource(source);
				}
			} while(ident >= 0);
		}

		static bool WaitForAndroidStartup(android_app *androidApp, AndroidState *androidState)
		{
			while(androidApp && !androidApp->destroyRequested && (!androidApp->window || androidApp->activityState != APP_CMD_RESUME))
			{
				ProcessAndroidEvents(androidState, -1);
			}

			if(androidApp && !androidApp->destroyRequested)
			{
				ANativeWindow *previousWindow = androidState->GetWindow();
				int32 previousActivityState = androidState->GetActivityState();
				bool previousDestroyRequested = androidState->GetDestroyRequested();

				androidState->SetApp(androidApp);
				androidState->SynchronizeState(previousWindow != androidApp->window,
											   previousActivityState != APP_CMD_RESUME && androidApp->activityState == APP_CMD_RESUME,
											   !previousDestroyRequested && (androidApp->destroyRequested != 0));
				return true;
			}

			return false;
		}

		static void DestroyAndroidState(AndroidState *androidState)
		{
			delete androidState;
		}
#endif

		static void TearDownKernel(Kernel *kernel)
		{
#if RN_PLATFORM_ANDROID
			const AndroidState *androidState = kernel->GetAndroidState();
			JavaVM *javaVM = androidState? androidState->GetJavaVM() : nullptr;
#endif

#if RN_PLATFORM_MAC_OS || RN_PLATFORM_IOS || RN_PLATFORM_VISIONOS
			@autoreleasepool
			{
				kernel->TearDown();
			}
#else
			kernel->TearDown();
#endif

#if RN_PLATFORM_ANDROID
			if(javaVM)
			{
				javaVM->DetachCurrentThread();
			}
#endif
		}
	};

	RNAPI Kernel *__BootstrapKernel(Application *app, const ArgumentParser &arguments, void *object
#if RN_PLATFORM_ANDROID
									 , AndroidState *androidState
#endif
	);
	RNAPI void __TearDownKernel(Kernel *kernel);

	Kernel *__BootstrapKernel(Application *app, const ArgumentParser &arguments, void *object
#if RN_PLATFORM_ANDROID
								, AndroidState *androidState
#endif
	)
	{
		Kernel *result = __KernelBootstrapHelper::BootstrapKernel(app, arguments, object
#if RN_PLATFORM_ANDROID
																 , androidState
#endif
		);
		return result;
	}

	void __TearDownKernel(Kernel *kernel)
	{
		__KernelBootstrapHelper::TearDownKernel(kernel);
	}

	void Initialize(int argc, const char *argv[], Application *app, void *object)
	{
		RN_ASSERT(app, "Application mustn't be NULL");

#if RN_ENABLE_VTUNE
		VTuneDomain = __itt_domain_create("Rayne");
		__itt_thread_set_nameA("Main thread");
#endif

#if RN_PLATFORM_MAC_OS
		@autoreleasepool
		{
			[RNApplication sharedApplication];
			[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
			[NSApp finishLaunching];

			[[RNApplication sharedApplication] setDelegate:(RNApplication *)[RNApplication sharedApplication]];
			[[RNApplication sharedApplication] activateIgnoringOtherApps:YES];

			@autoreleasepool
			{
				NSDate *date = [NSDate date];
				NSEvent *event;

				while((event = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:date inMode:NSDefaultRunLoopMode dequeue:YES]))
				{
					[NSApp sendEvent:event];
					[NSApp updateWindows];
				}

				NSMenu *menu = [[NSMenu alloc] init];
				[menu setAutoenablesItems:NO];

				NSString *quitTitle = [@"Quit " stringByAppendingString:[[NSProcessInfo processInfo] processName]];
				NSMenuItem *quitMenuItem = [[NSMenuItem alloc] initWithTitle:quitTitle action:@selector(terminate:) keyEquivalent:@"q"];
				NSMenu *quitMenu = [[NSMenu alloc] init];
				[quitMenu addItem:[quitMenuItem autorelease]];

				NSMenuItem *appMenu = [[NSMenuItem alloc] init];
				[appMenu setSubmenu:[quitMenu autorelease]];
				[menu addItem:[appMenu autorelease]];

				[NSApp setMainMenu:[menu autorelease]];
			}
		}
#endif

#if RN_PLATFORM_ANDROID
		std::cout.rdbuf(new AndroidBuffer(ANDROID_LOG_INFO));
		std::cerr.rdbuf(new AndroidBuffer(ANDROID_LOG_ERROR));
		std::cout.setf(std::ios::unitbuf);
		std::cerr.setf(std::ios::unitbuf);
#endif

		ArgumentParser arguments(argc, argv);

#if RN_PLATFORM_ANDROID
		android_app *androidApp = static_cast<android_app *>(object);
		RN_ASSERT(androidApp, "Object needs to be a pointer to the android_app object for Android builds.");
		prctl(PR_SET_NAME, (long)"RN::Android", 0, 0, 0);

		AndroidState *androidState = __KernelBootstrapHelper::CreateAndroidState(androidApp);
		if(!__KernelBootstrapHelper::WaitForAndroidStartup(androidApp, androidState))
		{
			androidApp->onAppCmd = nullptr;
			__KernelBootstrapHelper::DestroyAndroidState(androidState);
			return;
		}

		std::atomic<bool> didFinishGameThread(false);
		std::exception_ptr gameThreadException;
		ALooper *platformLooper = ALooper_forThread();
		Thread *gameThread = new Thread([&] {
			try
			{
				Kernel *result = __BootstrapKernel(app, arguments, object, androidState);
				result->Run();
				__TearDownKernel(result);
			}
			catch(...)
			{
				gameThreadException = std::current_exception();
			}

			didFinishGameThread.store(true, std::memory_order_release);
			if(platformLooper) ALooper_wake(platformLooper);
		});

		while(!didFinishGameThread.load(std::memory_order_acquire))
		{
			__KernelBootstrapHelper::ProcessAndroidEvents(androidState, -1);
		}

		gameThread->WaitForExit();
		gameThread->Release();

		if(!androidApp->destroyRequested && androidApp->activity)
		{
			ANativeActivity_finish(androidApp->activity);
		}

		androidApp->onAppCmd = nullptr;
		__KernelBootstrapHelper::DestroyAndroidState(androidState);

		if(gameThreadException)
		{
			std::rethrow_exception(gameThreadException);
		}

		std::_Exit(EXIT_SUCCESS);
#else
		Kernel *result = __BootstrapKernel(app, arguments, object);
#if RN_PLATFORM_MAC_OS
		[(RNApplication *)[RNApplication sharedApplication] setKernel:result];
#endif

		result->Run();

		__TearDownKernel(result);

		std::exit(EXIT_SUCCESS);
#endif
	}

	void __Assert(const char *func, const char *file, int line, const char *expression, const char *message, ...)
	{
		va_list args;
		va_start(args, message);

		char reason[1024];
		vsprintf(reason, message, args);
		reason[1023] = '\0';

		va_end(args);


		{
			RNError("Assertion '" << expression << "' failed in '" << func << "', " << file << ":" << line);
			RNError("Reason: " << reason);
		}

		abort();
	}

	uint32 GetABIVersion() RN_NOEXCEPT
	{
		return kRNABIVersion;
	}
	uint32 GetAPIVersion() RN_NOEXCEPT
	{
		return RNVersionMake(kRNVersionMajor, kRNVersionMinor, kRNVersionPatch);
	}

	uint32 GetMajorVersion() RN_NOEXCEPT
	{
		return kRNVersionMajor;
	}
	uint32 GetMinorVersion() RN_NOEXCEPT
	{
		return kRNVersionMinor;
	}
	uint32 GetPatchVersion() RN_NOEXCEPT
	{
		return kRNVersionPatch;
	}
	String *GetVersionString(uint32 version)
	{
		String *versionString = RNSTR(RNVersionGetMajor(version) << "." << RNVersionGetMinor(version));

		if(RNVersionGetPatch(version) > 0)
			versionString->Append(RNSTR("." << RNVersionGetPatch(version)));

		return versionString;
	}
} // namespace RN
