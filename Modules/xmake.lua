add_repositories("localrepo " .. path.absolute(path.join(os.scriptdir(), "xmake-packages")))

-- Metal on apple platforms
if has_config("rayne_build_metal") then
	includes("Metal/xmake.lua")
end

-- Vulkan
if has_config("rayne_build_vulkan") then
	includes("Vulkan/xmake.lua")
end

-- VR wrapper is always present (it conditionally depends on renderer)
includes("VRWrapper/xmake.lua")

-- OpenXR
if has_config("rayne_build_openxr") and not is_plat("iphoneos", "iphonesimulator", "applexros") then
	includes("OpenXR/xmake.lua")
end

-- AppleXR
if is_plat("applexros") then
	includes("AppleXR/xmake.lua")
end

-- Physics (Jolt)
if has_config("rayne_build_jolt") then
	includes("Jolt/xmake.lua")
end

-- Resonance Audio
includes("ResonanceAudio/xmake.lua")

-- OpenAL audio backend
if has_config("rayne_build_openal") then
	includes("OpenAL/xmake.lua")
end

-- Ogg asset loader
if has_config("rayne_build_ogg") then
	includes("ogg/xmake.lua")
end

-- UI module
if has_config("rayne_build_ui") then
	includes("UI/xmake.lua")
end

-- Epic Online Services
if has_config("rayne_build_eos") then
	includes("EpicOnlineServices/xmake.lua")
end

if has_config("rayne_build_physx") then
	includes("PhysX/xmake.lua")
end


