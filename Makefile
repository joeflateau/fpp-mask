SRCDIR ?= /opt/fpp/src
include $(SRCDIR)/makefiles/common/setup.mk
include $(SRCDIR)/makefiles/platform/*.mk

all: libfpp-mask.$(SHLIB_EXT)
debug: all

OBJECTS_fpp_mask_so += src/FPPMask.o
LIBS_fpp_mask_so    += -L$(SRCDIR) -lfpp -ljsoncpp
CXXFLAGS_src/FPPMask.o += -I$(SRCDIR)

%.o: %.cpp Makefile
	$(CCACHE) $(CC) $(CFLAGS) $(CXXFLAGS) $(CXXFLAGS_$@) -c $< -o $@

libfpp-mask.$(SHLIB_EXT): $(OBJECTS_fpp_mask_so) $(SRCDIR)/libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_mask_so) $(LIBS_fpp_mask_so) $(LDFLAGS) -o $@

clean:
	rm -f libfpp-mask.$(SHLIB_EXT) $(OBJECTS_fpp_mask_so)
