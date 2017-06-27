OBJS += x86/cpu.o                                                       \

OBJS-$(CONFIG_PIXELUTILS) += x86/pixelutils_init.o                      \

EMMS_OBJS_$(HAVE_MMX_INLINE)_$(HAVE_MMX_EXTERNAL)_$(HAVE_MM_EMPTY) = x86/emms.o

X86ASM-OBJS += x86/cpuid.o                                              \
             $(EMMS_OBJS__yes_)                                      \

X86ASM-OBJS-$(CONFIG_PIXELUTILS) += x86/pixelutils.o                    \
