cmake_minimum_required(VERSION 3.23.2)

project(bssh)

 if (DEFINED CMAKE_CXX_FLAGS_DEBUG_INIT AND  
     "${CMAKE_CXX_FLAGS_DEBUG_INIT}" STREQUAL "${CMAKE_CXX_FLAGS_DEBUG}")
     # Overwrite the init values choosen by CMake
     if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
         set(CMAKE_CXX_FLAGS_DEBUG "-ggdb3 -O0" CACHE STRING "" FORCE)
     endif()
 endif()

add_executable(${PROJECT_NAME}
	src/main.c
	src/toml.c
	src/toml_write.c
	src/net.c
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
)

target_compile_options(${PROJECT_NAME} PRIVATE -Wall)
