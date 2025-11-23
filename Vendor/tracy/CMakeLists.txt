include(FetchContent)

FetchContent_Declare(
	tracy
	GIT_REPOSITORY https://github.com/wolfpld/tracy.git
	GIT_TAG v0.13.0
	GIT_SHALLOW TRUE
	GIT_PROGRESS TRUE)

FetchContent_MakeAvailable(tracy)
