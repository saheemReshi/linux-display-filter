CXX ?= c++
PKG_CONFIG ?= pkg-config
CXXFLAGS ?= -O2 -g
OPENCV_CFLAGS = $(shell $(PKG_CONFIG) --cflags opencv5)
OPENCV_LIBS = $(shell $(PKG_CONFIG) --libs-only-L opencv5) \
	-lopencv_dnn -lopencv_imgcodecs -lopencv_imgproc -lopencv_core

all: detect-cats

detect-cats: main.o libcat-detector.a
	$(CXX) $(LDFLAGS) $^ $(OPENCV_LIBS) -o $@

libcat-detector.a: cat_detector.o cat_detector_c.o
	$(AR) rcs $@ $^

%.o: %.cpp
	$(PKG_CONFIG) --exists opencv5
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -std=c++17 -Wall -Wextra -Werror \
		$(OPENCV_CFLAGS) -c $< -o $@

main.o cat_detector.o: cat_detector.hpp
cat_detector_c.o: cat_detector_c.h cat_detector.hpp

.PHONY: all
