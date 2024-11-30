option(TRACY_ENABLE "" OFF)

FetchContent_Declare(
	tracy
	GIT_REPOSITORY https://github.com/wolfpld/tracy.git
	GIT_TAG 5d542dc09f3d9378d005092a4ad446bd405f819a #v0.11.1
	GIT_SHALLOW TRUE
	GIT_PROGRESS TRUE)

FetchContent_MakeAvailable(tracy)

target_compile_definitions(TracyClient PUBLIC $<$<CONFIG:RelWithDebInfo>:TRACY_ENABLE>)
