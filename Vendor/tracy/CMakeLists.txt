include(FetchContent)

set(OLD_BUILD_SHARED_LIBS "${BUILD_SHARED_LIBS}")
set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)

FetchContent_Declare(
	tracy
	GIT_REPOSITORY https://github.com/wolfpld/tracy.git
	GIT_TAG v0.13.0
	GIT_SHALLOW TRUE
	GIT_PROGRESS TRUE)

FetchContent_MakeAvailable(tracy)

set(BUILD_SHARED_LIBS "${OLD_BUILD_SHARED_LIBS}" CACHE BOOL "" FORCE)
