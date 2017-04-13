ARCH_HEADERS = mathops.h

# subsystems
OBJS-$(CONFIG_AC3DSP)                  += arm/ac3dsp_init_arm.o         \
                                          arm/ac3dsp_arm.o
OBJS-$(CONFIG_BLOCKDSP)                += arm/blockdsp_init_arm.o
OBJS-$(CONFIG_FFT)                     += arm/fft_init_arm.o            \
                                          arm/fft_fixed_init_arm.o
OBJS-$(CONFIG_FMTCONVERT)              += arm/fmtconvert_init_arm.o
OBJS-$(CONFIG_H264CHROMA)              += arm/h264chroma_init_arm.o
OBJS-$(CONFIG_H264DSP)                 += arm/h264dsp_init_arm.o
OBJS-$(CONFIG_H264PRED)                += arm/h264pred_init_arm.o
OBJS-$(CONFIG_H264QPEL)                += arm/h264qpel_init_arm.o
OBJS-$(CONFIG_HPELDSP)                 += arm/hpeldsp_init_arm.o        \
                                          arm/hpeldsp_arm.o
OBJS-$(CONFIG_IDCTDSP)                 += arm/idctdsp_init_arm.o        \
                                          arm/idctdsp_arm.o             \
                                          arm/jrevdct_arm.o             \
                                          arm/simple_idct_arm.o
OBJS-$(CONFIG_ME_CMP)                  += arm/me_cmp_init_arm.o
OBJS-$(CONFIG_MPEGAUDIODSP)            += arm/mpegaudiodsp_init_arm.o
OBJS-$(CONFIG_PIXBLOCKDSP)             += arm/pixblockdsp_init_arm.o
OBJS-$(CONFIG_VIDEODSP)                += arm/videodsp_init_arm.o

# decoders/encoders
OBJS-$(CONFIG_HEVC_DECODER)            += arm/hevcdsp_init_arm.o


# ARMv5 optimizations
# subsystems
ARMV5TE-OBJS-$(CONFIG_IDCTDSP)         += arm/idctdsp_init_armv5te.o    \
                                          arm/simple_idct_armv5te.o
ARMV5TE-OBJS-$(CONFIG_MPEGVIDEO)       += arm/mpegvideo_armv5te.o       \
                                          arm/mpegvideo_armv5te_s.o
ARMV5TE-OBJS-$(CONFIG_VIDEODSP)        += arm/videodsp_init_armv5te.o   \
                                          arm/videodsp_armv5te.o

# ARMv6 optimizations
# subsystems
ARMV6-OBJS-$(CONFIG_HPELDSP)           += arm/hpeldsp_init_armv6.o      \
                                          arm/hpeldsp_armv6.o
ARMV6-OBJS-$(CONFIG_IDCTDSP)           += arm/idctdsp_init_armv6.o      \
                                          arm/idctdsp_armv6.o           \
                                          arm/simple_idct_armv6.o
ARMV6-OBJS-$(CONFIG_ME_CMP)            += arm/me_cmp_armv6.o
ARMV6-OBJS-$(CONFIG_MPEGAUDIODSP)      += arm/mpegaudiodsp_fixed_armv6.o
ARMV6-OBJS-$(CONFIG_PIXBLOCKDSP)       += arm/pixblockdsp_armv6.o
ARMV6-OBJS-$(CONFIG_STARTCODE)         += arm/startcode_armv6.o

# VFP optimizations

# subsystems
VFP-OBJS-$(CONFIG_FFT)                 += arm/fft_vfp.o
VFP-OBJS-$(CONFIG_FMTCONVERT)          += arm/fmtconvert_vfp.o
VFP-OBJS-$(CONFIG_MDCT)                += arm/mdct_vfp.o

# NEON optimizations

# subsystems
NEON-OBJS-$(CONFIG_BLOCKDSP)           += arm/blockdsp_init_neon.o      \
                                          arm/blockdsp_neon.o
NEON-OBJS-$(CONFIG_FFT)                += arm/fft_neon.o                \
                                          arm/fft_fixed_neon.o
NEON-OBJS-$(CONFIG_FMTCONVERT)         += arm/fmtconvert_neon.o
NEON-OBJS-$(CONFIG_H264CHROMA)         += arm/h264cmc_neon.o
NEON-OBJS-$(CONFIG_H264DSP)            += arm/h264dsp_neon.o            \
                                          arm/h264idct_neon.o
NEON-OBJS-$(CONFIG_H264PRED)           += arm/h264pred_neon.o
NEON-OBJS-$(CONFIG_H264QPEL)           += arm/h264qpel_neon.o           \
                                          arm/hpeldsp_neon.o
NEON-OBJS-$(CONFIG_HPELDSP)            += arm/hpeldsp_init_neon.o       \
                                          arm/hpeldsp_neon.o
NEON-OBJS-$(CONFIG_IDCTDSP)            += arm/idctdsp_init_neon.o       \
                                          arm/idctdsp_neon.o            \
                                          arm/simple_idct_neon.o
NEON-OBJS-$(CONFIG_MDCT)               += arm/mdct_neon.o               \
                                          arm/mdct_fixed_neon.o
NEON-OBJS-$(CONFIG_MPEGVIDEO)          += arm/mpegvideo_neon.o
NEON-OBJS-$(CONFIG_RDFT)               += arm/rdft_neon.o

# decoders/encoders
NEON-OBJS-$(CONFIG_HEVC_DECODER)       += arm/hevcdsp_init_neon.o       \
                                          arm/hevcdsp_deblock_neon.o    \
                                          arm/hevcdsp_idct_neon.o       \
                                          arm/hevcdsp_qpel_neon.o		\
                                          arm/hevcdsp_epel_neon.o		\
                                          arm/hevcdsp_sao_neon.o		\
                                          arm/hevcdsp_il_pred_neon.o
