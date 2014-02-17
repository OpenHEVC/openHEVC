include ../../config.mak

vpath %.c $(SRC_PATH)/modules/openhevc_dec

CFLAGS=$(OPTFLAGS) -I"$(SRC_PATH)/include" $(ffmpeg_lflags)

ifeq ($(CONFIG_LINUX), yes)
LDFLAGS+=-Wl,-Bsymbolic
endif

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

EXTRALIBS+= -L../../bin/gcc -lgpac $(ffmpeg_lflags)

#common obj
OBJS= openhevc_dec.o openHevcWrapper.o

SRCS := $(OBJS:.o=.c) 

LIB=gm_openhevc_dec.$(DYN_LIB_SUFFIX)


all: $(LIB)


$(LIB): $(OBJS)
	$(CC) $(SHFLAGS) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJS) $(EXTRALIBS)

clean: 
	rm -f $(OBJS) ../../bin/gcc/$(LIB)

dep: depend

depend:
	rm -f .depend	
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

distclean: clean
	rm -f Makefile.bak .depend

-include .depend
