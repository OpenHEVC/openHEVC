include $(SUBDIR)../config.mak

NAME = avcodec

HEADERS = avcodec.h                                                     \
          avdct.h                                                       \
          avfft.h                                                       \
          d3d11va.h                                                     \
          dirac.h                                                       \
          dv_profile.h                                                  \
          dxva2.h                                                       \
          jni.h                                                         \
          mediacodec.h                                                  \
          qsv.h                                                         \
          vaapi.h                                                       \
          vda.h                                                         \
          vdpau.h                                                       \
          version.h                                                     \
          videotoolbox.h                                                \
          vorbis_parser.h                                               \
          xvmc.h                                                        \

OBJS = allcodecs.o                                                      \
       avpacket.o                                                       \
       avpicture.o                                                      \
       bitstream.o                                                      \
       bitstream_filter.o                                               \
       bitstream_filters.o                                              \
       bsf.o                                                            \
       codec_desc.o                                                     \
       mathtables.o                                                     \
       options.o                                                        \
       parser.o                                                         \
       profiles.o                                                       \
       raw.o                                                            \
       utils.o                                                          \


# subsystems
OBJS-$(CONFIG_AANDCTTABLES)            += aandcttab.o
OBJS-$(CONFIG_AC3DSP)                  += ac3dsp.o
OBJS-$(CONFIG_BLOCKDSP)                += blockdsp.o
OBJS-$(CONFIG_BSWAPDSP)                += bswapdsp.o
OBJS-$(CONFIG_CABAC)                   += cabac.o
OBJS-$(CONFIG_DCT)                     += dct.o dct32_fixed.o dct32_float.o
OBJS-$(CONFIG_ERROR_RESILIENCE)        += error_resilience.o
OBJS-$(CONFIG_FAANDCT)                 += faandct.o
OBJS-$(CONFIG_FAANIDCT)                += faanidct.o
OBJS-$(CONFIG_FDCTDSP)                 += fdctdsp.o jfdctfst.o jfdctint.o
FFT-OBJS-$(CONFIG_HARDCODED_TABLES)    += cos_tables.o cos_fixed_tables.o
OBJS-$(CONFIG_FFT)                     += avfft.o fft_fixed.o fft_float.o \
                                          fft_fixed_32.o fft_init_table.o \
                                          $(FFT-OBJS-yes)
OBJS-$(CONFIG_GOLOMB)                  += golomb.o
OBJS-$(CONFIG_H264CHROMA)              += h264chroma.o
OBJS-$(CONFIG_H264DSP)                 += h264dsp.o h264idct.o
OBJS-$(CONFIG_H264PRED)                += h264pred.o
OBJS-$(CONFIG_H264QPEL)                += h264qpel.o
OBJS-$(CONFIG_HPELDSP)                 += hpeldsp.o
OBJS-$(CONFIG_IDCTDSP)                 += idctdsp.o simple_idct.o jrevdct.o
OBJS-$(CONFIG_INTRAX8)                 += intrax8.o intrax8dsp.o
OBJS-$(CONFIG_MDCT)                    += mdct_fixed.o mdct_float.o mdct_fixed_32.o
OBJS-$(CONFIG_ME_CMP)                  += me_cmp.o
OBJS-$(CONFIG_MPEG_ER)                 += mpeg_er.o
OBJS-$(CONFIG_PIXBLOCKDSP)             += pixblockdsp.o
OBJS-$(CONFIG_QPELDSP)                 += qpeldsp.o
OBJS-$(CONFIG_RDFT)                    += rdft.o $(RDFT-OBJS-yes)
OBJS-$(CONFIG_SHARED)                  += log2_tab.o reverse.o
OBJS-$(CONFIG_STARTCODE)               += startcode.o
OBJS-$(CONFIG_VIDEODSP)                += videodsp.o


# decoders/encoders
OBJS-$(CONFIG_H264_DECODER)            += h264dec.o h264_cabac.o h264_cavlc.o \
                                          h264_direct.o h264_loopfilter.o  \
                                          h264_mb.o h264_picture.o h264_ps.o \
                                          h264_refs.o h264_sei.o \
                                          h264_slice.o h264data.o h264_parse.o \
                                          h2645_parse.o
OBJS-$(CONFIG_H264_CUVID_DECODER)      += cuvid.o

OBJS-$(CONFIG_H264_VDA_DECODER)        += vda_h264_dec.o

OBJS-$(CONFIG_HEVC_DECODER)            += hevc.o hevc_mvs.o hevc_ps.o hevc_sei.o \
                                          hevc_cabac.o hevc_refs.o hevcpred.o    \
                                          hevcdsp.o hevc_filter.o h2645_parse.o 








# hardware accelerators
OBJS-$(CONFIG_D3D11VA)                    += dxva2.o
OBJS-$(CONFIG_DXVA2)                      += dxva2.o
OBJS-$(CONFIG_VAAPI)                      += vaapi.o
OBJS-$(CONFIG_VDA)                        += vda.o videotoolbox.o
OBJS-$(CONFIG_VIDEOTOOLBOX)               += videotoolbox.o
OBJS-$(CONFIG_VDPAU)                      += vdpau.o

OBJS-$(CONFIG_H264_D3D11VA_HWACCEL)       += dxva2_h264.o
OBJS-$(CONFIG_H264_DXVA2_HWACCEL)         += dxva2_h264.o
OBJS-$(CONFIG_H264_VAAPI_HWACCEL)         += vaapi_h264.o
OBJS-$(CONFIG_H264_VDA_HWACCEL)           += vda_h264.o
OBJS-$(CONFIG_H264_VDPAU_HWACCEL)         += vdpau_h264.o
OBJS-$(CONFIG_H264_VIDEOTOOLBOX_HWACCEL)  += videotoolbox.o
OBJS-$(CONFIG_HEVC_D3D11VA_HWACCEL)       += dxva2_hevc.o
OBJS-$(CONFIG_HEVC_DXVA2_HWACCEL)         += dxva2_hevc.o
OBJS-$(CONFIG_HEVC_VAAPI_HWACCEL)         += vaapi_hevc.o
OBJS-$(CONFIG_HEVC_VDPAU_HWACCEL)         += vdpau_hevc.o



# libavformat dependencies
OBJS-$(CONFIG_ISO_MEDIA)               += mpeg4audio.o mpegaudiodata.o
OBJS-$(CONFIG_MOV_DEMUXER)             += ac3tab.o



# parsers
OBJS-$(CONFIG_H264_PARSER)             += h264_parser.o h264_parse.o h2645_parse.o \
                                          h264_ps.o h264_sei.o h264data.o
OBJS-$(CONFIG_HEVC_PARSER)             += hevc_parser.o h2645_parse.o hevc_ps.o 


# bitstream filters
OBJS-$(CONFIG_CHOMP_BSF)                  += chomp_bsf.o
OBJS-$(CONFIG_DUMP_EXTRADATA_BSF)         += dump_extradata_bsf.o
OBJS-$(CONFIG_H264_MP4TOANNEXB_BSF)       += h264_mp4toannexb_bsf.o
OBJS-$(CONFIG_HEVC_MP4TOANNEXB_BSF)       += hevc_mp4toannexb_bsf.o
OBJS-$(CONFIG_REMOVE_EXTRADATA_BSF)       += remove_extradata_bsf.o


# thread libraries
OBJS-$(HAVE_LIBC_MSVCRT)               += file_open.o
OBJS-$(HAVE_THREADS)                   += pthread.o pthread_slice.o pthread_frame.o
OBJS-$(CONFIG_FRAME_THREAD_ENCODER)    += frame_thread_encoder.o

# Windows resource file
SLIBOBJS-$(HAVE_GNU_WINDRES)           += avcodecres.o

SKIPHEADERS-$(CONFIG_D3D11VA)          += d3d11va.h dxva2_internal.h
SKIPHEADERS-$(CONFIG_DXVA2)            += dxva2.h dxva2_internal.h
SKIPHEADERS-$(CONFIG_QSV)              += qsv.h qsv_internal.h
SKIPHEADERS-$(CONFIG_VAAPI)            += vaapi_encode.h vaapi_internal.h
SKIPHEADERS-$(CONFIG_VDA)              += vda.h vda_vt_internal.h
SKIPHEADERS-$(CONFIG_VDPAU)            += vdpau.h vdpau_internal.h
SKIPHEADERS-$(CONFIG_VIDEOTOOLBOX)     += videotoolbox.h vda_vt_internal.h


