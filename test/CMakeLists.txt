cmake_minimum_required(VERSION 3.23.2)

include(C:/Users/henry/AppData/Roaming/bssh/Basilisk/CMakeLists.txt)
project(test)

add_executable(${PROJECT_NAME}
    src/main.c
)

target_include_directories(${PROJECT_NAME}
    PUBLIC include/
    PUBLIC C:/Users/henry/AppData/Roaming/bssh/Basilisk/include/basilisk
)

target_link_directories(${PROJECT_NAME}
    PRIVATE build
)

target_link_libraries(${PROJECT_NAME}
    basilisk
    opengl32
)

target_compile_options(${PROJECT_NAME} PRIVATE -Wall)
