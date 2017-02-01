include $(SUBDIR)../config.mak

NAME = avutil

HEADERS = adler32.h                                                     \
          aes.h                                                         \
          aes_ctr.h                                                     \
          attributes.h                                                  \
          audio_fifo.h                                                  \
          avassert.h                                                    \
          avstring.h                                                    \
          avutil.h                                                      \
          base64.h                                                      \
          blowfish.h                                                    \
          bprint.h                                                      \
          bswap.h                                                       \
          buffer.h                                                      \
          cast5.h                                                       \
          camellia.h                                                    \
          channel_layout.h                                              \
          common.h                                                      \
          cpu.h                                                         \
          crc.h                                                         \
          des.h                                                         \
          dict.h                                                        \
          display.h                                                     \
          downmix_info.h                                                \
          error.h                                                       \
          eval.h                                                        \
          fifo.h                                                        \
          file.h                                                        \
          frame.h                                                       \
          hash.h                                                        \
          hmac.h                                                        \
          hwcontext.h                                                   \
          hwcontext_cuda.h                                              \
          hwcontext_dxva2.h                                             \
          hwcontext_vaapi.h                                             \
          hwcontext_vdpau.h                                             \
          imgutils.h                                                    \
          intfloat.h                                                    \
          intreadwrite.h                                                \
          lfg.h                                                         \
          log.h                                                         \
          macros.h                                                      \
          mathematics.h                                                 \
          mastering_display_metadata.h                                  \
          md5.h                                                         \
          mem.h                                                         \
          motion_vector.h                                               \
          murmur3.h                                                     \
          opt.h                                                         \
          parseutils.h                                                  \
          pixdesc.h                                                     \
          pixelutils.h                                                  \
          pixfmt.h                                                      \
          random_seed.h                                                 \
          rc4.h                                                         \
          rational.h                                                    \
          replaygain.h                                                  \
          ripemd.h                                                      \
          samplefmt.h                                                   \
          sha.h                                                         \
          sha512.h                                                      \
          stereo3d.h                                                    \
          threadmessage.h                                               \
          time.h                                                        \
          timecode.h                                                    \
          timestamp.h                                                   \
          tree.h                                                        \
          twofish.h                                                     \
          version.h                                                     \
          xtea.h                                                        \
          tea.h                                                         \

HEADERS-$(CONFIG_LZO)                   += lzo.h

HEADERS-$(CONFIG_OPENCL)                += opencl.h

ARCH_HEADERS = bswap.h                                                  \
               intmath.h                                                \
               intreadwrite.h                                           \
               timer.h                                                  \

BUILT_HEADERS = avconfig.h                                              \
                ffversion.h

OBJS = aes.o                                                            \
       aes_ctr.o                                                        \
       avstring.o                                                       \
       base64.o                                                         \
       bprint.o                                                         \
       buffer.o                                                         \
       channel_layout.o                                                 \
       cpu.o                                                            \
       crc.o                                                            \
       des.o                                                            \
       dict.o                                                           \
       display.o                                                        \
       error.o                                                          \
       eval.o                                                           \
       fifo.o                                                           \
       file.o                                                           \
       file_open.o                                                      \
       frame.o                                                          \
       hmac.o                                                           \
       imgutils.o                                                       \
       intmath.o                                                        \
       lfg.o                                                            \
       log.o                                                            \
       log2_tab.o                                                       \
       mathematics.o                                                    \
       mastering_display_metadata.o                                     \
       md5.o                                                            \
       mem.o                                                            \
       opt.o                                                            \
       parseutils.o                                                     \
       pixdesc.o                                                        \
       random_seed.o                                                    \
       rational.o                                                       \
       reverse.o                                                        \
       rc4.o                                                            \
       samplefmt.o                                                      \
       sha.o                                                            \
       sha512.o                                                         \
       stereo3d.o                                                       \
       time.o                                                           \
       timecode.o                                                       \
       tree.o                                                           \
       utils.o                                                          \


OBJS-$(!HAVE_ATOMICS_NATIVE)            += atomic.o

OBJS-$(CONFIG_CUDA)                     += hwcontext_cuda.o
OBJS-$(CONFIG_DXVA2)                    += hwcontext_dxva2.o
OBJS-$(CONFIG_OPENCL)                   += opencl.o opencl_internal.o
OBJS-$(CONFIG_VAAPI)                    += hwcontext_vaapi.o
OBJS-$(CONFIG_VDPAU)                    += hwcontext_vdpau.o

OBJS += $(COMPAT_OBJS:%=../compat/%)

# Windows resource file
SLIBOBJS-$(HAVE_GNU_WINDRES)            += avutilres.o

SKIPHEADERS-$(CONFIG_CUDA)             += hwcontext_cuda.h
SKIPHEADERS-$(CONFIG_DXVA2)            += hwcontext_dxva2.h
SKIPHEADERS-$(CONFIG_VAAPI)            += hwcontext_vaapi.h
SKIPHEADERS-$(CONFIG_VDPAU)            += hwcontext_vdpau.h
SKIPHEADERS-$(HAVE_ATOMICS_GCC)        += atomic_gcc.h
SKIPHEADERS-$(HAVE_ATOMICS_SUNCC)      += atomic_suncc.h
SKIPHEADERS-$(HAVE_ATOMICS_WIN32)      += atomic_win32.h
SKIPHEADERS-$(CONFIG_OPENCL)           += opencl.h

