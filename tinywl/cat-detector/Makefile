CXX ?= c++
PKG_CONFIG ?= pkg-config
CXXFLAGS ?= -O2 -g

all: detect-cats

detect-cats: main.cpp cat_detector.cpp cat_detector.hpp
	$(PKG_CONFIG) --exists opencv5
	$(CXX) $(CXXFLAGS) -std=c++17 -Wall -Wextra -Werror main.cpp cat_detector.cpp \
		$(shell $(PKG_CONFIG) --cflags --libs-only-L opencv5) \
		-lopencv_dnn -lopencv_imgcodecs -lopencv_imgproc -lopencv_core -o $@

.PHONY: all
