PKG_CONFIG?=pkg-config
CXX?=c++

PKGS="wlroots-0.20" wayland-server xkbcommon glesv2 pixman-1
CFLAGS_PKG_CONFIG!=$(PKG_CONFIG) --cflags $(PKGS)
CFLAGS+=$(CFLAGS_PKG_CONFIG)
LIBS!=$(PKG_CONFIG) --libs $(PKGS)
CAT_LIBS!=$(PKG_CONFIG) --libs-only-L opencv5
CAT_LIBS+=-lopencv_dnn -lopencv_imgproc -lopencv_core
CAT_SOURCES=cat-detector/cat_detector.cpp cat-detector/cat_detector.hpp \
	cat-detector/cat_detector_c.cpp cat-detector/cat_detector_c.h \
	cat-detector/readback_bgr.c cat-detector/readback_bgr.h cat-detector/Makefile

all: tinywl

cat-detector/libcat-detector.a: $(CAT_SOURCES)
	$(MAKE) -C cat-detector libcat-detector.a

tinywl.o: tinywl.c cat-detector/cat_detector_c.h cat-detector/readback_bgr.h
	$(CC) -c $< -g -Werror $(CFLAGS) -I. -DWLR_USE_UNSTABLE -o $@
tinywl: tinywl.o cat-detector/libcat-detector.a
	$(CXX) $^ -g -Werror $(CFLAGS) $(LDFLAGS) $(LIBS) $(CAT_LIBS) -o $@

clean:
	rm -f tinywl tinywl.o

.PHONY: all clean
