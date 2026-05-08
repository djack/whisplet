cmake_minimum_required(VERSION 3.20)
project(whisplet)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)

set(WHISPER_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS      OFF CACHE BOOL "" FORCE)  # statically link whisper + ggml

if(APPLE)
    set(GGML_METAL ON CACHE BOOL "" FORCE)   # GPU on Apple Silicon / AMD Mac
else()
    set(GGML_VULKAN ON CACHE BOOL "" FORCE)  # GPU on Linux
endif()

FetchContent_Declare(
    whisper_cpp
    GIT_REPOSITORY https://github.com/ggml-org/whisper.cpp.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(whisper_cpp)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)

add_executable(whisplet main.cpp)
target_include_directories(whisplet PRIVATE ${PORTAUDIO_INCLUDE_DIRS})
target_link_libraries(whisplet PRIVATE whisper ${PORTAUDIO_LIBRARIES})
target_compile_options(whisplet PRIVATE ${PORTAUDIO_CFLAGS_OTHER})

if(APPLE)
    target_link_libraries(whisplet PRIVATE
        "-framework ApplicationServices"
        "-framework Carbon"
    )
else()
    find_package(X11 REQUIRED)
    target_include_directories(whisplet PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(whisplet PRIVATE ${X11_LIBRARIES})
endif()
