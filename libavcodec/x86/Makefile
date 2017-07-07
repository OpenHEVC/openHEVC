OBJS                                   += x86/constants.o               \

# subsystems
OBJS-$(CONFIG_AC3DSP)                  += x86/ac3dsp_init.o
OBJS-$(CONFIG_BLOCKDSP)                += x86/blockdsp_init.o
OBJS-$(CONFIG_BSWAPDSP)                += x86/bswapdsp_init.o
OBJS-$(CONFIG_DCT)                     += x86/dct_init.o
OBJS-$(CONFIG_FDCTDSP)                 += x86/fdctdsp_init.o
OBJS-$(CONFIG_FFT)                     += x86/fft_init.o
OBJS-$(CONFIG_FMTCONVERT)              += x86/fmtconvert_init.o
OBJS-$(CONFIG_H264CHROMA)              += x86/h264chroma_init.o
OBJS-$(CONFIG_H264DSP)                 += x86/h264dsp_init.o
OBJS-$(CONFIG_H264PRED)                += x86/h264_intrapred_init.o
OBJS-$(CONFIG_H264QPEL)                += x86/h264_qpel.o

OBJS-$(CONFIG_HPELDSP)                 += x86/hpeldsp_init.o
OBJS-$(CONFIG_IDCTDSP)                 += x86/idctdsp_init.o
OBJS-$(CONFIG_ME_CMP)                  += x86/me_cmp_init.o
OBJS-$(CONFIG_MPEGAUDIODSP)            += x86/mpegaudiodsp.o

OBJS-$(CONFIG_PIXBLOCKDSP)             += x86/pixblockdsp_init.o
OBJS-$(CONFIG_QPELDSP)                 += x86/qpeldsp_init.o
OBJS-$(CONFIG_VIDEODSP)                += x86/videodsp_init.o
OBJS-$(CONFIG_XMM_CLOBBER_TEST)        += x86/w64xmmtest.o

# decoders/encoders
OBJS-$(CONFIG_HEVC_DECODER)            += x86/hevcpred_init.o          \
                                          x86/hevcdsp_init.o


# GCC inline assembly optimizations
# subsystems
MMX-OBJS-$(CONFIG_FDCTDSP)             += x86/fdct.o
MMX-OBJS-$(CONFIG_IDCTDSP)             += x86/simple_idct.o

SSE-OBJS-$(CONFIG_HEVC_DECODER)        += x86/hevc_idct_sse.o          \
                                          x86/hevc_il_pred_sse.o       \
                                          x86/hevc_intra_pred_sse.o    \
                                          x86/hevc_sao_sse.o           \

ifeq (CONFIG_OH_OPTIM,0)
SSE-OBJS-$(CONFIG_HEVC_DECODER)        += x86/hevc_mc_sse.o
endif
#TODO fix broken x86_mc_sse
AVX-OBJS-$(CONFIG_HEVC_DECODER)        += x86/hevc_mc_avx2.o  

# subsystems
YASM-OBJS-$(CONFIG_BLOCKDSP)           += x86/blockdsp.o
YASM-OBJS-$(CONFIG_BSWAPDSP)           += x86/bswapdsp.o
YASM-OBJS-$(CONFIG_DCT)                += x86/dct32.o
YASM-OBJS-$(CONFIG_FFT)                += x86/fft.o
YASM-OBJS-$(CONFIG_H264CHROMA)         += x86/h264_chromamc.o           \
                                          x86/h264_chromamc_10bit.o
YASM-OBJS-$(CONFIG_H264DSP)            += x86/h264_deblock.o            \
                                          x86/h264_deblock_10bit.o      \
                                          x86/h264_idct.o               \
                                          x86/h264_idct_10bit.o         \
                                          x86/h264_weight.o             \
                                          x86/h264_weight_10bit.o
YASM-OBJS-$(CONFIG_H264PRED)           += x86/h264_intrapred.o          \
                                          x86/h264_intrapred_10bit.o
YASM-OBJS-$(CONFIG_H264QPEL)           += x86/h264_qpel_8bit.o          \
                                          x86/h264_qpel_10bit.o         \
                                          x86/fpel.o                    \
                                          x86/qpel.o
YASM-OBJS-$(CONFIG_HPELDSP)            += x86/fpel.o                    \
                                          x86/hpeldsp.o
YASM-OBJS-$(CONFIG_IDCTDSP)            += x86/idctdsp.o
YASM-OBJS-$(CONFIG_ME_CMP)             += x86/me_cmp.o
YASM-OBJS-$(CONFIG_PIXBLOCKDSP)        += x86/pixblockdsp.o
YASM-OBJS-$(CONFIG_QPELDSP)            += x86/qpeldsp.o                 \
                                          x86/fpel.o                    \
                                          x86/qpel.o
YASM-OBJS-$(CONFIG_IDCTDSP)            += x86/simple_idct10.o
YASM-OBJS-$(CONFIG_VIDEODSP)           += x86/videodsp.o


# decoders/encoders
YASM-OBJS-$(CONFIG_HEVC_DECODER)       += x86/hevc_mc.o                 \
                                          x86/hevc_deblock.o            \
                                          x86/hevc_idct.o               \
                                          x86/hevc_res_add.o

