CXX ?= g++
CXXFLAGS ?= -Wall -Wextra -O2 -fPIC
INCLUDES = -Iinclude
LDFLAGS = -Llib -Wl,-rpath,'$$ORIGIN'
LDLIBS = -lhcnetsdk

LIB_TARGET = libhcnet_thermal_picture.so
PY_TARGET = hcnet_thermal_picture.py

all: $(LIB_TARGET)

$(LIB_TARGET): main.cpp
	$(CXX) $(CXXFLAGS) -shared $(INCLUDES) -o $@ $< $(LDFLAGS) $(LDLIBS)

wheel: $(LIB_TARGET) $(PY_TARGET) pyproject.toml
	python -m pip wheel . -w dist

clean:
	rm -f $(LIB_TARGET)
	rm -rf build dist *.egg-info

.PHONY: all clean wheel
