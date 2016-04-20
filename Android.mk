LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
APP_STL := gnustl_static
APP_ABI := armeabi
APP_OPTIM := debug
LOCAL_ARM_MODE := arm

#LOCAL_CFLAGS    := -I <Your header files goes here>
#for debug
#LOCAL_CFLAGS    := -g -ggdb -O0
#for production
LOCAL_CFLAGS    := -O3

openhevc_files := \
    libavutil/avstring.c \
    libavutil/atomic.c \
    libavutil/base64.c \
    libavutil/bprint.c \
    libavutil/buffer.c \
    libavutil/channel_layout.c \
    libavutil/cpu.c \
    libavutil/crc.c \
    libavutil/des.c \
    libavutil/dict.c \
    libavutil/display.c \
    libavutil/error.c \
    libavutil/eval.c \
    libavutil/file_open.c \
    libavutil/frame.c \
    libavutil/imgutils.c \
    libavutil/intmath.c \
    libavutil/log.c \
    libavutil/log2_tab.c \
    libavutil/mathematics.c \
    libavutil/md5.c \
    libavutil/mem.c \
    libavutil/opt.c \
    libavutil/parseutils.c \
    libavutil/pixdesc.c \
    libavutil/rational.c \
    libavutil/random_seed.c \
    libavutil/rc4.c \
    libavutil/samplefmt.c \
    libavutil/sha.c \
    libavutil/stereo3d.c \
    libavutil/time.c \
    libavutil/timecode.c \
    libavutil/utils.c \
    libavutil/arm/cpu.c \
    gpac/modules/openhevc_dec/openHevcWrapper.c \
    libavformat/allformats.c \
    libavformat/avio.c \
    libavformat/aviobuf.c \
    libavformat/cutils.c \
    libavformat/file.c \
    libavformat/flac_picture.c \
    libavformat/format.c \
    libavformat/h264dec.c \
    libavformat/id3v1.c \
    libavformat/id3v2.c \
    libavformat/isom.c \
    libavformat/hevcdec.c \
    libavformat/matroska.c \
    libavformat/matroskadec.c \
    libavformat/metadata.c \
    libavformat/mov.c \
    libavformat/mov_chan.c \
    libavformat/mpegts.c \
    libavformat/mux.c \
    libavformat/oggparsevorbis.c \
    libavformat/options.c \
    libavformat/os_support.c \
    libavformat/rawdec.c \
    libavformat/replaygain.c \
    libavformat/riffdec.c \
    libavformat/riff.c \
    libavformat/rmsipr.c \
    libavformat/utils.c \
    libavformat/vorbiscomment.c \
    libavcodec/h264_cabac.c \
    libavcodec/h264_cavlc.c \
    libavcodec/h264_direct.c \
    libavcodec/h264_loopfilter.c \
    libavcodec/h264_mb.c \
    libavcodec/h264_parser.c \
    libavcodec/h264_picture.c \
    libavcodec/h264_ps.c \
    libavcodec/h264_refs.c \
    libavcodec/h264_sei.c \
    libavcodec/h264_slice.c \
    libavcodec/h264.c \
    libavcodec/h264chroma.c \
    libavcodec/h264dsp.c \
    libavcodec/h264idct.c \
    libavcodec/h264pred.c \
    libavcodec/h264qpel.c \
    libavcodec/arm/videodsp_init_arm.c \
    libavcodec/ac3tab.c \
    libavcodec/allcodecs.c \
    libavcodec/avfft.c \
    libavcodec/avpacket.c \
    libavcodec/avpicture.c \
    libavcodec/bitstream.c \
    libavcodec/bitstream_filter.c \
    libavcodec/bswapdsp.c \
    libavcodec/cabac.c \
    libavcodec/codec_desc.c \
    libavcodec/dct.c \
    libavcodec/dct32_float.c \
    libavcodec/dct32_template.c \
    libavcodec/faanidct.c \
    libavcodec/fft_template.c \
    libavcodec/golomb.c \
    libavcodec/hevc_cabac.c \
    libavcodec/hevc_mvs.c \
    libavcodec/hevc_parser.c \
    libavcodec/hevc_ps.c \
    libavcodec/hevc_refs.c \
    libavcodec/hevc_sei.c \
    libavcodec/hevc_filter.c \
    libavcodec/hevc.c \
    libavcodec/hevcdsp.c \
    libavcodec/hevcpred.c \
    libavcodec/hpeldsp.c \
    libavcodec/jrevdct.c \
    libavcodec/mathtables.c \
    libavcodec/me_cmp.c \
    libavcodec/mdct_template.c \
    libavcodec/mpegaudiodata.c \
    libavcodec/mpeg4audio.c \
    libavcodec/imgconvert.c \
    libavcodec/options.c \
    libavcodec/parser.c \
    libavcodec/pthread_slice.c \
    libavcodec/pthread_frame.c \
    libavcodec/pthread.c \
    libavcodec/raw.c \
    libavcodec/rawdec.c \
    libavcodec/rdft.c \
    libavcodec/simple_idct.c \
    libavcodec/startcode.c \
    libavcodec/utils.c \
    libavcodec/videodsp.c \
    libavcodec/arm/hevcdsp_init_arm.c \
    libavutil/arm/asm.S \
    libavcodec/arm/hevcdsp_deblock_neon.S \
    libavcodec/arm/hevcdsp_idct_neon.S \
    libavcodec/arm/hevcdsp_qpel_neon.S \
    libavcodec/arm/hevcdsp_epel_neon.S \
    libavcodec/arm/simple_idct_neon.S \
    libavcodec/arm/simple_idct_arm.S \
    libavcodec/arm/simple_idct_armv6.S \
    libavcodec/arm/jrevdct_arm.S \
    libavcodec/arm/int_neon.S \
    main_hm/getopt.c \
    main_hm/main.c

LOCAL_SRC_FILES := $(openhevc_files)
LOCAL_C_INCLUDES += $(LOCAL_PATH)/platform/arm/ \
                    $(LOCAL_PATH)/gpac/modules/openhevc_dec/
LOCAL_MODULE := openhevc
ifeq ($(TARGET_ARCH),arm)
  LOCAL_SDK_VERSION := 9
endif
include $(BUILD_EXECUTABLE)
