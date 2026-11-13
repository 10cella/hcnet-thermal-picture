CXX ?= g++
AR ?= ar
CXXFLAGS ?= -Wall -Wextra -O2 -std=c++11 -fPIC
CPPFLAGS ?= -Iinclude
SDK_LIB_DIR ?= lib
SDK_LIB_NAME ?= hcnetsdk
BUILD_DIR ?= build

STATIC_LIB = $(BUILD_DIR)/libhcnet_thermal.a
SHARED_LIB = $(BUILD_DIR)/libhcnet_thermal.so
OBJECT = $(BUILD_DIR)/hcnet_thermal.o
EXAMPLE = $(BUILD_DIR)/hcnet_capture_example

SDK_LIBS = $(wildcard lib/*.so)
SDK_COMPONENT_LIBS = $(wildcard lib/HCNetSDKCom/*.so)

.PHONY: all static shared example wheel clean

all: static shared

static: $(STATIC_LIB)

shared: $(SHARED_LIB)

example: $(EXAMPLE)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(OBJECT): src/hcnet_thermal.cpp include/hcnet_thermal.h | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(STATIC_LIB): $(OBJECT)
	$(AR) rcs $@ $^

$(SHARED_LIB): $(OBJECT)
	$(CXX) -shared -o $@ $^ -L$(SDK_LIB_DIR) -l$(SDK_LIB_NAME) \
		-Wl,-rpath,'$$ORIGIN/../$(SDK_LIB_DIR)'

$(EXAMPLE): examples/capture_cli.cpp $(STATIC_LIB) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ $< $(STATIC_LIB) \
		-L$(SDK_LIB_DIR) -l$(SDK_LIB_NAME) \
		-Wl,-rpath,'$$ORIGIN/../$(SDK_LIB_DIR)'

wheel: $(SHARED_LIB)
	cp $(SHARED_LIB) hcnet_thermal_picture/
	cp $(SDK_LIBS) hcnet_thermal_picture/
	mkdir -p hcnet_thermal_picture/HCNetSDKCom
	cp $(SDK_COMPONENT_LIBS) hcnet_thermal_picture/HCNetSDKCom/
	python -m pip wheel . -w dist

clean:
	rm -rf $(BUILD_DIR)
	rm -f hcnet_thermal_picture/libhcnet_thermal.so
	rm -f hcnet_thermal_picture/*.so
	rm -rf hcnet_thermal_picture/HCNetSDKCom
	rm -rf dist *.egg-info
