#!/usr/bin/make -f

PREFIX ?= /usr/local
LIBDIR ?= lib
LV2DIR ?= $(PREFIX)/$(LIBDIR)/lv2

OPTIMIZATIONS ?= -msse -msse2 -mfpmath=sse -ffast-math -fomit-frame-pointer -O3 -fno-finite-math-only

LDFLAGS ?= -Wl,--as-needed
CXXFLAGS ?= $(OPTIMIZATIONS) -Wall

###############################################################################
BUNDLE = zamcomp.lv2

CXXFLAGS += -fPIC -DPIC

UNAME=$(shell uname)
ifeq ($(UNAME),Darwin)
  LIB_EXT=.dylib
  LDFLAGS += -dynamiclib
else
  LDFLAGS += -shared -Wl,-Bstatic -Wl,-Bdynamic
  LIB_EXT=.so
endif


ifeq ($(shell pkg-config --exists lv2 lv2core lv2-plugin || echo no), no)
  $(error "LV2 SDK was not found")
else
  LV2FLAGS=`pkg-config --cflags --libs lv2 lv2core lv2-plugin`
endif

ifeq ($(shell pkg-config --exists lv2-gui || echo no), no)
  $(error "LV2-GUI is required ")
else
  LV2GUIFLAGS=`pkg-config --cflags --libs lv2-gui lv2 lv2core lv2-plugin`
endif


$(BUNDLE): manifest.ttl zamcomp.ttl zamcomp$(LIB_EXT) zamcomp_gui$(LIB_EXT)
	rm -rf $(BUNDLE)
	mkdir $(BUNDLE)
	cp manifest.ttl zamcomp.ttl zamcomp$(LIB_EXT) $(BUNDLE)

zamcomp$(LIB_EXT): zamcomp.cpp
	$(CXX) -o zamcomp$(LIB_EXT) \
		$(CXXFLAGS) $(LV2FLAGS) $(LDFLAGS) \
		zamcomp.cpp

zamcomp_gui$(LIB_EXT): zamcomp_gui.cpp zamcomp.peg
	$(CXX) -o zamcomp_gui$(LIB_EXT) \
		$(CXXFLAGS) $(LV2GUIFLAGS) $(LDFLAGS) \
		zamcomp_gui.cpp

zamcomp.peg: zamcomp.ttl
	lv2peg zamcomp.ttl zamcomp.peg

install: $(BUNDLE)
	install -d $(DESTDIR)$(LV2DIR)/$(BUNDLE)
	install -t $(DESTDIR)$(LV2DIR)/$(BUNDLE) $(BUNDLE)/*
	install zamcomp_gui$(LIB_EXT) $(DESTDIR)$(LV2DIR)/$(BUNDLE)

uninstall:
	rm -rf $(DESTDIR)$(LV2DIR)/$(BUNDLE)

clean:
	rm -rf $(BUNDLE) zamcomp$(LIB_EXT) zamcomp_gui$(LIB_EXT) zamcomp.peg

.PHONY: clean install uninstall
