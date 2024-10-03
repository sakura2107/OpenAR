cmake_minimum_required(VERSION 3.20.0)

option(ENABLE_CUDA ON)

if(ENABLE_CUDA)
    add_definitions(-DENABLE_CUDA)
endif()

if(ENABLE_CUDA)
    project(OpenAR LANGUAGES CXX CUDA)
else()
    project(OpenAR LANGUAGES CXX)
endif()


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(VCPKG_ROOT "D:/vcpkg-2024.08.23")
set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(ENABLE_CUDA)
    find_package(CUDA REQUIRED)
endif()
find_package(OpenCV REQUIRED)
find_package(Tesseract REQUIRED)

if(ENABLE_CUDA)
    add_subdirectory(Cuda)
endif()

add_subdirectory(FrameWork)

add_subdirectory(PCRDemo)
