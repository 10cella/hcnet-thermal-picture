cmake_minimum_required(VERSION 3.15)
project(hcnet_thermal LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(HCNET_SDK_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include"
    CACHE PATH "Directory containing HCNetSDK.h")
set(HCNET_SDK_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib"
    CACHE PATH "Directory containing the HCNetSDK library")

if(WIN32)
    set(HCNET_SDK_LIB_NAME "HCNetSDK" CACHE STRING "HCNetSDK import library name")
else()
    set(HCNET_SDK_LIB_NAME "hcnetsdk" CACHE STRING "HCNetSDK library name")
endif()

set(HCNET_THERMAL_JPEG_PIC_SIZE "0xff"
    CACHE STRING "NET_DVR_JPEGPARA.wPicSize: 0xff=auto, 6=VGA 640x480, 2=4CIF")

function(hcnet_thermal_configure_target target)
    target_include_directories(${target}
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
        PRIVATE "${HCNET_SDK_INCLUDE_DIR}")
    target_compile_definitions(${target}
        PRIVATE HCNET_THERMAL_JPEG_PIC_SIZE=${HCNET_THERMAL_JPEG_PIC_SIZE})
    target_link_directories(${target} PUBLIC "${HCNET_SDK_LIB_DIR}")
    target_link_libraries(${target} PUBLIC "${HCNET_SDK_LIB_NAME}")
    if(WIN32)
        target_link_libraries(${target} PUBLIC ws2_32 winmm iphlpapi shlwapi)
    endif()
endfunction()

add_library(hcnet_thermal_static STATIC src/hcnet_thermal.cpp)
set_target_properties(hcnet_thermal_static PROPERTIES
    OUTPUT_NAME hcnet_thermal
    POSITION_INDEPENDENT_CODE ON)
hcnet_thermal_configure_target(hcnet_thermal_static)
add_library(hcnet_thermal::static ALIAS hcnet_thermal_static)

option(HCNET_THERMAL_BUILD_SHARED "Build the shared library used by Python ctypes" ON)
if(HCNET_THERMAL_BUILD_SHARED)
    add_library(hcnet_thermal_shared SHARED src/hcnet_thermal.cpp)
    set_target_properties(hcnet_thermal_shared PROPERTIES
        OUTPUT_NAME hcnet_thermal
        POSITION_INDEPENDENT_CODE ON)
    if(WIN32)
        # Keep the static .lib and the shared library's import .lib from colliding.
        set_target_properties(hcnet_thermal_shared PROPERTIES
            ARCHIVE_OUTPUT_NAME hcnet_thermal_shared)
    endif()
    target_compile_definitions(hcnet_thermal_shared PRIVATE HCNET_THERMAL_EXPORTS)
    hcnet_thermal_configure_target(hcnet_thermal_shared)
    add_library(hcnet_thermal::shared ALIAS hcnet_thermal_shared)
endif()

option(HCNET_THERMAL_BUILD_EXAMPLE "Build the C API example executable" ON)
if(HCNET_THERMAL_BUILD_EXAMPLE)
    add_executable(hcnet_capture_example examples/capture_cli.cpp)
    target_link_libraries(hcnet_capture_example PRIVATE hcnet_thermal_static)
endif()

include(GNUInstallDirs)
install(TARGETS hcnet_thermal_static
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
if(HCNET_THERMAL_BUILD_SHARED)
    install(TARGETS hcnet_thermal_shared
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()
install(FILES include/hcnet_thermal.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
