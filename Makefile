CXX ?= g++
AR ?= ar
CXXFLAGS ?= -Wall -Wextra -O2 -std=c++11 -fPIC
SDK_INCLUDE_DIR ?= include
SDK_LIB_DIR ?= lib
SDK_LIB_NAME ?= hcnetsdk
BUILD_DIR ?= build

# Project and SDK headers. Kept separate from CPPFLAGS so an environment
# CPPFLAGS value cannot drop -Iinclude by accident.
INCLUDES = -Iinclude -I$(SDK_INCLUDE_DIR)

STATIC_LIB = $(BUILD_DIR)/libhcnet_thermal.a
OBJECTS = $(BUILD_DIR)/hcnet_thermal.o $(BUILD_DIR)/uvc_capture.o
CLI = $(BUILD_DIR)/hcnet_thermal_cli

# UVC support is optional: auto-detected with pkg-config, UVC=0 disables it,
# UVC=1 requires OpenCV 4 to be installed.
UVC ?= auto
ifeq ($(UVC),0)
OPENCV_PKG :=
else ifeq ($(UVC),1)
OPENCV_PKG := opencv4
else
OPENCV_PKG := $(shell pkg-config --exists opencv4 2>/dev/null && echo opencv4)
endif

ifeq ($(OPENCV_PKG),opencv4)
UVC_CPPFLAGS := $(shell pkg-config --cflags opencv4)
UVC_LIBS := $(shell pkg-config --libs opencv4)
UVC_DEFINE := -DHCNET_THERMAL_HAVE_UVC=1
else
UVC_CPPFLAGS :=
UVC_LIBS :=
UVC_DEFINE := -DHCNET_THERMAL_HAVE_UVC=0
endif

# Only embed a relative rpath when the SDK lives in a relative directory.
RPATH_FLAG = $(if $(filter /%,$(SDK_LIB_DIR)),,-Wl,-rpath,'$$ORIGIN/../$(SDK_LIB_DIR)')

.PHONY: all static cli clean

all: static cli

static: $(STATIC_LIB)

cli: $(CLI)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/hcnet_thermal.o: src/hcnet_thermal.cpp include/hcnet_thermal.h | $(BUILD_DIR)
	$(CXX) $(INCLUDES) $(CXXFLAGS) $(UVC_DEFINE) -c $< -o $@

$(BUILD_DIR)/uvc_capture.o: src/uvc_capture.cpp include/hcnet_thermal.h | $(BUILD_DIR)
	$(CXX) $(INCLUDES) $(CXXFLAGS) $(UVC_DEFINE) $(UVC_CPPFLAGS) -c $< -o $@

$(STATIC_LIB): $(OBJECTS)
	$(AR) rcs $@ $^

$(CLI): cli/main.cpp $(STATIC_LIB) | $(BUILD_DIR)
	$(CXX) $(INCLUDES) $(CXXFLAGS) -o $@ cli/main.cpp $(STATIC_LIB) $(UVC_LIBS) \
		-L$(SDK_LIB_DIR) -l$(SDK_LIB_NAME) $(RPATH_FLAG)

clean:
	rm -rf $(BUILD_DIR)
