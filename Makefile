CXX ?= g++
CXXFLAGS ?= -Wall -Wextra -O2
INCLUDES = -Iinclude
LDFLAGS = -Llib -Wl,-rpath,'$$ORIGIN/lib'
LDLIBS = -lhcnetsdk

CLI_TARGET = hcnet_capture_cli
SDK_LIBS = $(wildcard lib/*.so)
SDK_COMPONENT_LIBS = $(wildcard lib/HCNetSDKCom/*.so)

all: $(CLI_TARGET)

$(CLI_TARGET): capture_cli.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $< $(LDFLAGS) $(LDLIBS)

wheel: $(CLI_TARGET) $(SDK_LIBS) $(SDK_COMPONENT_LIBS) pyproject.toml hcnet_thermal_picture/__init__.py hcnet_thermal_picture/_api.py
	cp $(SDK_LIBS) hcnet_thermal_picture/
	mkdir -p hcnet_thermal_picture/HCNetSDKCom
	cp $(SDK_COMPONENT_LIBS) hcnet_thermal_picture/HCNetSDKCom/
	python -m pip wheel . -w dist

clean:
	rm -f $(CLI_TARGET)
	rm -f hcnet_thermal_picture/*.so
	rm -rf hcnet_thermal_picture/HCNetSDKCom
	rm -rf build dist *.egg-info

.PHONY: all clean wheel
