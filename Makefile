CXX ?= g++
CXXFLAGS ?= -Wall -Wextra -O2 -fPIC
INCLUDES = -Iinclude
LDFLAGS = -Llib -Wl,-rpath,'$$ORIGIN'
LDLIBS = -lhcnetsdk

LIB_TARGET = hcnet_thermal_picture/libhcnet_thermal_picture.so
SDK_LIBS = $(wildcard lib/*.so)

all: $(LIB_TARGET)

$(LIB_TARGET): main.cpp
	mkdir -p hcnet_thermal_picture
	$(CXX) $(CXXFLAGS) -shared $(INCLUDES) -o $@ $< $(LDFLAGS) $(LDLIBS)

wheel: $(LIB_TARGET) $(SDK_LIBS) pyproject.toml hcnet_thermal_picture/__init__.py hcnet_thermal_picture/_api.py
	cp $(SDK_LIBS) hcnet_thermal_picture/
	python -m pip wheel . -w dist

clean:
	rm -f $(LIB_TARGET)
	rm -f hcnet_thermal_picture/*.so
	rm -rf build dist *.egg-info

.PHONY: all clean wheel
