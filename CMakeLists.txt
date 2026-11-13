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

option(HCNET_THERMAL_ENABLE_UVC
    "Enable UVC capture support in the library (requires OpenCV)" ON)

set(HCNET_THERMAL_HAVE_UVC 0)
if(HCNET_THERMAL_ENABLE_UVC)
    find_package(OpenCV QUIET COMPONENTS core imgcodecs videoio)
    if(OpenCV_FOUND)
        set(HCNET_THERMAL_HAVE_UVC 1)
        message(STATUS "UVC capture enabled (OpenCV ${OpenCV_VERSION})")
    else()
        message(STATUS
            "OpenCV not found; library built without UVC support")
    endif()
endif()

function(hcnet_thermal_configure_target target)
    target_include_directories(${target}
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
        PRIVATE "${HCNET_SDK_INCLUDE_DIR}")
    target_compile_definitions(${target}
        PRIVATE
            HCNET_THERMAL_JPEG_PIC_SIZE=${HCNET_THERMAL_JPEG_PIC_SIZE}
            HCNET_THERMAL_HAVE_UVC=${HCNET_THERMAL_HAVE_UVC})
    target_link_directories(${target} PUBLIC "${HCNET_SDK_LIB_DIR}")
    target_link_libraries(${target} PUBLIC "${HCNET_SDK_LIB_NAME}")
    if(WIN32)
        target_link_libraries(${target} PUBLIC ws2_32 winmm iphlpapi shlwapi)
    endif()
endfunction()

# The static library is the integration point for Rust and other native callers.
add_library(hcnet_thermal_static STATIC
    src/hcnet_thermal.cpp
    src/uvc_capture.cpp)
set_target_properties(hcnet_thermal_static PROPERTIES
    OUTPUT_NAME hcnet_thermal
    POSITION_INDEPENDENT_CODE ON)
hcnet_thermal_configure_target(hcnet_thermal_static)
if(HCNET_THERMAL_HAVE_UVC)
    target_include_directories(hcnet_thermal_static PRIVATE ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(hcnet_thermal_static PUBLIC ${OpenCV_LIBS})
endif()
add_library(hcnet_thermal::static ALIAS hcnet_thermal_static)

option(HCNET_THERMAL_BUILD_CLI "Build the hcnet_thermal_cli executable" ON)
if(HCNET_THERMAL_BUILD_CLI)
    add_executable(hcnet_thermal_cli cli/main.cpp)
    target_link_libraries(hcnet_thermal_cli PRIVATE hcnet_thermal_static)
endif()

include(GNUInstallDirs)
install(TARGETS hcnet_thermal_static
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
if(HCNET_THERMAL_BUILD_CLI)
    install(TARGETS hcnet_thermal_cli
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()
install(FILES include/hcnet_thermal.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
