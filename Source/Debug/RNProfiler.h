//
//  RNProfiler.h
//  Rayne
//
//  Copyright 2025
//

#ifndef __RAYNE_PROFILER_H__
#define __RAYNE_PROFILER_H__

// Select exactly one backend by defining one of:
//   - RN_PROFILE_TRACY
//
// If none are defined, all profiling macros become no-ops.

// ---------------------------
// Tracy backend
// ---------------------------
#if defined(RN_PROFILE_TRACY)
	#include <tracy/Tracy.hpp>

	#define RN_PROFILE_SCOPE()            ZoneScoped
	#define RN_PROFILE_SCOPE_N(name)      ZoneScopedN(name)

	#define RN_PROFILE_FRAME() FrameMark
	#define RN_PROFILE_FRAME_TRACY() FrameMark

	// ---------------------------
	// GPU (API-specific defines: Metal & Vulkan) - Tracy
	// Note: Ensure the respective Tracy GPU header is included in the translation
	// unit using these (TracyMetal.hpp / TracyVulkan.hpp).
	// ---------------------------
	/*#if RN_BUILD_METAL
		//Requires ARC, but then other things break.
		#include <tracy/TracyMetal.hmm>
		#define RN_PROFILE_METAL_CONTEXT_TYPE TracyMetalCtx

		#define RN_PROFILE_METAL_DECLARE_CONTEXT(ctxVar, device) TracyMetalContext(device)
		#define RN_PROFILE_METAL_COLLECT(ctxVar) TracyMetalCollect(ctxVar)
		#define RN_PROFILE_METAL_SCOPE(ctxVar, cmdBuf) TracyMetalZone(ctxVar, cmdBuf, __FUNCTION__)
		#define RN_PROFILE_METAL_SCOPE_N(ctxVar, cmdBuf, name) TracyMetalZone(ctxVar, cmdBuf, name)
		#define RN_PROFILE_METAL_DESTROY_CONTEXT(ctxVar) TracyMetalDestroy(ctxVar)
	#else*/
		#define RN_PROFILE_METAL_CONTEXT_TYPE void*
		#define RN_PROFILE_METAL_DECLARE_CONTEXT(ctxVar, device) nullptr
		#define RN_PROFILE_METAL_COLLECT(ctxVar) ((void)0)
		#define RN_PROFILE_METAL_SCOPE(ctxVar, cmdBuf) ((void)0)
		#define RN_PROFILE_METAL_SCOPE_N(ctxVar, cmdBuf, name) ((void)0)
		#define RN_PROFILE_METAL_DESTROY_CONTEXT(ctxVar) ((void)0)
	//#endif

	#if RN_BUILD_VULKAN
		#include <vulkan.h>
		#define TRACY_VK_USE_SYMBOL_TABLE 1
		#include <tracy/TracyVulkan.hpp>
		#define RN_PROFILE_VULKAN_CONTEXT_TYPE TracyVkCtx

		#define RN_PROFILE_VULKAN_DECLARE_CONTEXT(instance, device, physicalDevice, queue, cmdBuf, instanceProcAddr, deviceProcAddr) TracyVkContext(instance, physicalDevice, device, queue, cmdBuf, instanceProcAddr, deviceProcAddr)
		#define RN_PROFILE_VULKAN_COLLECT(ctxVar, cmdBuf) TracyVkCollect(ctxVar, cmdBuf)
		#define RN_PROFILE_VULKAN_SCOPE_CMD(ctxVar, cmdBuf) TracyVkZone(ctxVar, cmdBuf, __FUNCTION__)
		#define RN_PROFILE_VULKAN_SCOPE_CMD_N(ctxVar, cmdBuf, name) TracyVkZone(ctxVar, cmdBuf, name)
		#define RN_PROFILE_VULKAN_DESTROY_CONTEXT(ctxVar) TracyVkDestroy(ctxVar)
	#else
		#define RN_PROFILE_VULKAN_CONTEXT_TYPE void*
		#define RN_PROFILE_VULKAN_DECLARE_CONTEXT(instance, device, physicalDevice, queue, cmdBuf, instanceProcAddr, deviceProcAddr) nullptr
		#define RN_PROFILE_VULKAN_COLLECT(ctxVar, cmdBuf) ((void)0)
		#define RN_PROFILE_VULKAN_SCOPE_CMD(ctxVar, cmdBuf) ((void)0)
		#define RN_PROFILE_VULKAN_SCOPE_CMD_N(ctxVar, cmdBuf, name) ((void)0)
		#define RN_PROFILE_VULKAN_DESTROY_CONTEXT(ctxVar) ((void)0)
	#endif

// ---------------------------
// No-op backend
// ---------------------------
#else
	#define RN_PROFILE_SCOPE()            ((void)0)
	#define RN_PROFILE_SCOPE_N(name)      ((void)0)

	#define RN_PROFILE_FRAME() ((void)0)
	#define RN_PROFILE_FRAME_TRACY() ((void)0)

	#define RN_PROFILE_METAL_CONTEXT_TYPE void*
	#define RN_PROFILE_METAL_DECLARE_CONTEXT(ctxVar, device, cmdBuf) nullptr
	#define RN_PROFILE_METAL_COLLECT(ctxVar) ((void)0)
	#define RN_PROFILE_METAL_SCOPE(ctxVar, cmdBuf) ((void)0)
	#define RN_PROFILE_METAL_SCOPE_N(ctxVar, cmdBuf, name) ((void)0)
	#define RN_PROFILE_METAL_DESTROY_CONTEXT(ctxVar) ((void)0)

	#define RN_PROFILE_VULKAN_CONTEXT_TYPE void*
	#define RN_PROFILE_VULKAN_DECLARE_CONTEXT(instance, device, physicalDevice, queue, cmdBuf, instanceProcAddr, deviceProcAddr) nullptr
	#define RN_PROFILE_VULKAN_COLLECT(ctxVar, cmdBuf) ((void)0)
	#define RN_PROFILE_VULKAN_SCOPE_CMD(ctxVar, cmdBuf) ((void)0)
	#define RN_PROFILE_VULKAN_SCOPE_CMD_N(ctxVar, cmdBuf, name) ((void)0)
	#define RN_PROFILE_VULKAN_DESTROY_CONTEXT(ctxVar) ((void)0)
#endif

#endif /* __RAYNE_PROFILER_H__ */


