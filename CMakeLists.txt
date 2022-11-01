cmake_minimum_required(VERSION 3.23.2)

project(bssh)

add_executable(${PROJECT_NAME}
	src/main.c
	src/cJSON.c
)

add_subdirectory(libgit2)

target_include_directories(${PROJECT_NAME}
	PUBLIC include/
	PUBLIC libgit2/include
)

target_link_directories(${PROJECT_NAME}
	PRIVATE lib
	PRIVATE external/libgit2
)

target_link_libraries(${PROJECT_NAME}
	git2
	winhttp
	ws2_32
	rpcrt4
	z
	crypt32
)

target_compile_options(${PROJECT_NAME} PRIVATE -Wall)
