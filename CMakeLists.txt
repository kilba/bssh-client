cmake_minimum_required(VERSION 3.23.2)

include(FetchContent)
FetchContent_Declare(cpr GIT_REPOSITORY https://github.com/libcpr/cpr.git
                         GIT_TAG 0817715923c9705e68994eb52ef9df3f6845beba) # The commit hash for 1.10.x. Replace with the latest from: https://github.com/libcpr/cpr/releases
FetchContent_MakeAvailable(cpr)

project(bssh)

add_executable(${PROJECT_NAME}
	src/main.cpp
	src/help.cpp
	src/sha1.c
	src/base64.c
	src/qrcodegen.c
)

add_subdirectory(external/libgit2)

target_include_directories(${PROJECT_NAME}
	PUBLIC include/
	PUBLIC external/libgit2/include
)

target_link_directories(${PROJECT_NAME}
	PRIVATE lib
	PRIVATE build/external/libgit2
)

target_link_libraries(${PROJECT_NAME}
	git2
	winhttp
	ws2_32
	rpcrt4
	z
	crypt32
	secur32
	cpr::cpr
)

target_compile_options(${PROJECT_NAME} PRIVATE -Wall)
