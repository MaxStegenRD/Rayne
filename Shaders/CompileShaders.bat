cd /D "%~dp0"
start py -3 ../Tools/ShaderProcessor/convert.py Shaders.json spirv ../Modules/Vulkan/Resources ":RayneVulkan:"
