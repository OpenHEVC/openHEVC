MAIN_MAKEFILE=1
include config.mak

vpath %.c    $(SRC_PATH)
vpath %.cpp  $(SRC_PATH)
vpath %.h    $(SRC_PATH)
vpath %.inc  $(SRC_PATH)
vpath %.m    $(SRC_PATH)
vpath %.S    $(SRC_PATH)
vpath %.asm  $(SRC_PATH)
vpath %.rc   $(SRC_PATH)
vpath %.v    $(SRC_PATH)
vpath %.texi $(SRC_PATH)
vpath %/fate_config.sh.template $(SRC_PATH)

AVPROGS-$(CONFIG_OHPLAY)   += ohplay

AVPROGS    := $(AVPROGS-yes:%=%$(PROGSSUF)$(EXESUF))
INSTPROGS   = $(AVPROGS-yes:%=%$(PROGSSUF)$(EXESUF))
PROGS      += $(AVPROGS)

AVBASENAMES  = ohplay
ALLAVPROGS   = $(AVBASENAMES:%=%$(PROGSSUF)$(EXESUF))
ALLAVPROGS_G = $(AVBASENAMES:%=%$(PROGSSUF)_g$(EXESUF))

#$(foreach prog,$(AVBASENAMES),$(eval OBJS-$(prog) += cmdutils.o))

#OBJS-ffmpeg                   += ffmpeg_opt.o ffmpeg_filter.o
#OBJS-ffmpeg-$(CONFIG_VIDEOTOOLBOX) += ffmpeg_videotoolbox.o
#OBJS-ffmpeg-$(CONFIG_LIBMFX)  += ffmpeg_qsv.o
#OBJS-ffmpeg-$(CONFIG_VAAPI)   += ffmpeg_vaapi.o
#ifndef CONFIG_VIDEOTOOLBOX
#OBJS-ffmpeg-$(CONFIG_VDA)     += ffmpeg_videotoolbox.o
#endif
#OBJS-ffmpeg-$(CONFIG_CUVID)   += ffmpeg_cuvid.o
#OBJS-ffmpeg-$(HAVE_DXVA2_LIB) += ffmpeg_dxva2.o
#OBJS-ffmpeg-$(HAVE_VDPAU_X11) += ffmpeg_vdpau.o
#OBJS-ffserver                 += ffserver_config.o

# OpenHEVC Simple Player 
OBJS-ohplay-${CONFIG_SDL} += ohplay_utils/ohtimer_sdl.o ohplay_utils/ohdisplay_sdl.o
OBJS-ohplay-${CONFIG_SDL2} += ohplay_utils/ohtimer_sdl.o ohplay_utils/ohdisplay_sdl2.o
OBJS-ohplay-${CONFIG_NOVIDEO} += ohplay_utils/ohtimer_sys.o ohplay_utils/ohdisplay_none.o
OBJS-ohplay                   += cmdutils.o

TESTTOOLS   = audiogen videogen rotozoom tiny_psnr tiny_ssim base64 audiomatch
HOSTPROGS  := $(TESTTOOLS:%=tests/%) doc/print_options
TOOLS       = qt-faststart trasher uncoded_frame
TOOLS-$(CONFIG_ZLIB) += cws2fws

# $(FFLIBS-yes) needs to be in linking order
FFLIBS-$(CONFIG_AVFORMAT)   += avformat
FFLIBS-$(CONFIG_AVCODEC)    += avcodec
FFLIBS-$(CONFIG_OPENHEVC)   += openhevc

FFLIBS := avutil

include $(SRC_PATH)/common.mak

FF_EXTRALIBS := $(FFEXTRALIBS)
FF_DEP_LIBS  := $(DEP_LIBS)
FF_STATIC_DEP_LIBS := $(STATIC_DEP_LIBS)

all: $(AVPROGS)

$(TOOLS): %$(EXESUF): %.o $(EXEOBJS)
	$(LD) $(LDFLAGS) $(LDEXEFLAGS) $(LD_O) $^ $(ELIBS)

tools/cws2fws$(EXESUF): ELIBS = $(ZLIB)
tools/uncoded_frame$(EXESUF): $(FF_DEP_LIBS)
tools/uncoded_frame$(EXESUF): ELIBS = $(FF_EXTRALIBS)

config.h: .config
.config: $(wildcard $(FFLIBS:%=$(SRC_PATH)/lib%/all*.c))
	@-tput bold 2>/dev/null
	@-printf '\nWARNING: $(?F) newer than config.h, rerun configure\n\n'
	@-tput sgr0 2>/dev/null

SUBDIR_VARS := CLEANFILES EXAMPLES FFLIBS HOSTPROGS TESTPROGS TOOLS      \
               HEADERS ARCH_HEADERS BUILT_HEADERS SKIPHEADERS            \
               ARMV5TE-OBJS ARMV6-OBJS ARMV8-OBJS VFP-OBJS NEON-OBJS     \
               ALTIVEC-OBJS MMX-OBJS YASM-OBJS SSE-OBJS AVX-OBJS         \
               MIPSFPU-OBJS MIPSDSPR2-OBJS MIPSDSP-OBJS MSA-OBJS         \
               MMI-OBJS OBJS SLIBOBJS HOSTOBJS TESTOBJS

define RESET
$(1) :=
$(1)-yes :=
endef

define DOSUBDIR
$(foreach V,$(SUBDIR_VARS),$(eval $(call RESET,$(V))))
SUBDIR := $(1)/
include $(SRC_PATH)/$(1)/Makefile
-include $(SRC_PATH)/$(1)/$(ARCH)/Makefile
-include $(SRC_PATH)/$(1)/$(INTRINSICS)/Makefile
include $(SRC_PATH)/library.mak
endef

$(foreach D,$(FFLIBS),$(eval $(call DOSUBDIR,lib$(D))))

define DOPROG
OBJS-$(1) += $(1).o $(EXEOBJS) $(OBJS-$(1)-yes)
$(1)$(PROGSSUF)_g$(EXESUF): $$(OBJS-$(1))
$$(OBJS-$(1)): CFLAGS  += $(CFLAGS-$(1))
$(1)$(PROGSSUF)_g$(EXESUF): LDFLAGS += $(LDFLAGS-$(1))
$(1)$(PROGSSUF)_g$(EXESUF): FF_EXTRALIBS += $(LIBS-$(1))
-include $$(OBJS-$(1):.o=.d)
endef

$(foreach P,$(PROGS),$(eval $(call DOPROG,$(P:$(PROGSSUF)$(EXESUF)=))))

# cmdutils.o 
libavcodec/utils.o libavformat/utils.o libavutil/utils.o : libavutil/ffversion.h

$(PROGS): %$(PROGSSUF)$(EXESUF): %$(PROGSSUF)_g$(EXESUF)
	$(CP) $< $@
	$(STRIP) $@

%$(PROGSSUF)_g$(EXESUF): %.o $(FF_DEP_LIBS)
	$(LD) $(LDFLAGS) $(LDEXEFLAGS) $(LD_O) $(OBJS-$*) $(FF_EXTRALIBS)

OBJDIRS += tools

-include $(wildcard tools/*.d)

VERSION_SH  = $(SRC_PATH)/version.sh
GIT_LOG     = $(SRC_PATH)/.git/logs/HEAD

.version: $(wildcard $(GIT_LOG)) $(VERSION_SH) config.mak
.version: M=@

libavutil/ffversion.h .version:
	$(M)$(VERSION_SH) $(SRC_PATH) libavutil/ffversion.h $(EXTRA_VERSION)
	$(Q)touch .version

# force version.sh to run whenever version might have changed
-include .version

ifdef AVPROGS
install: install-progs install-data
endif

install: install-libs install-headers

install-libs: install-libs-yes

install-progs-yes:
install-progs-$(CONFIG_SHARED): install-libs

install-progs: install-progs-yes $(AVPROGS)
	$(Q)mkdir -p "$(BINDIR)"
	$(INSTALL) -c -m 755 $(INSTPROGS) "$(BINDIR)"

uninstall: uninstall-libs uninstall-headers uninstall-progs

uninstall-progs:
	$(RM) $(addprefix "$(BINDIR)/", $(ALLAVPROGS))

uninstall-data:
	$(RM) -r "$(DATADIR)"

clean::
	$(RM) $(ALLAVPROGS) $(ALLAVPROGS_G)
	$(RM) $(CLEANSUFFIXES)
	$(RM) $(CLEANSUFFIXES:%=tools/%)
	$(RM) -r coverage-html
	$(RM) -rf coverage.info coverage.info.in lcov

distclean::
	$(RM) $(DISTCLEANSUFFIXES)
	$(RM) config.* .config libavutil/avconfig.h .version mapfile avversion.h version.h libavutil/ffversion.h libavcodec/codec_names.h libavcodec/bsf_list.c libavformat/protocol_list.c
ifeq ($(SRC_LINK),src)
	$(RM) src
endif
	$(RM) -rf doc/examples/pc-uninstalled

config:
	$(SRC_PATH)/configure $(value FFMPEG_CONFIGURATION)

check: all alltools fate


$(sort $(OBJDIRS)):
	$(Q)mkdir -p $@

# Dummy rule to stop make trying to rebuild removed or renamed headers
%.h:
	@:

# Disable suffix rules.  Most of the builtin rules are suffix rules,
# so this saves some time on slow systems.
.SUFFIXES:

.PHONY: all all-yes alltools check *clean config install*
.PHONY: testprogs uninstall*
