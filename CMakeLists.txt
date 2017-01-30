cmake_minimum_required (VERSION 2.8)

# Configure build type (Release when no one is selected)
if (NOT DEFINED CMAKE_BUILD_TYPE)
  set (CMAKE_BUILD_TYPE Release CACHE STRING "Build type")
endif ()

project (openHEVC)
SET(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/MyCMakeScripts)

message("DEBUG: CMAKE_SYSTEM_PROCESSOR = ${CMAKE_SYSTEM_PROCESSOR}")
message("DEBUG: CMAKE_SYSTEM_NAME      = ${CMAKE_SYSTEM_NAME}")

macro (my_check_function_exists arg result)
  check_function_exists(${arg} ${result})
  if(${result} STREQUAL "")
    set(${result} 0)
  endif()
endmacro (my_check_function_exists)

macro (my_check_include_files arg result)
  check_include_files(${arg} ${result})
  if(${result} STREQUAL "")
    set(${result} 0)
  endif()
endmacro (my_check_include_files)

include(CheckTypeSize)
check_type_size("void*" SIZEOF_VOID_P BUILTIN_TYPES_ONLY)
message("DEBUG: SIZEOF_VOID_P      = ${SIZEOF_VOID_P}")
include(CheckFunctionExists)
include(CheckIncludeFiles)
include(OptimizeForArchitecture)

OptimizeForArchitecture()

my_check_function_exists(GetProcessAffinityMask GETPROCESSAFFINITYMASK_FOUND)
my_check_function_exists(gettimeofday           GETTIMEOFDAY_FOUND)
my_check_function_exists(sched_getaffinity      SCHED_GETAFFINITY_FOUND)
my_check_function_exists(strerror_r             STRERROR_R_FOUND)
my_check_function_exists(sysconf                SYSCONF_FOUND)
my_check_function_exists(usleep                 USLEEP_FOUND)
my_check_function_exists(localtime_r            LOCALTIME_R_FOUND)
my_check_function_exists(gmtime_r               GMTIME_R_FOUND)

my_check_include_files(fcntl.h                  FCNTL_H_FOUND)
my_check_include_files(pthread.h                PTHREADS_FOUND)
my_check_include_files(unistd.h                 UNISTD_H_FOUND)
my_check_include_files(windows.h                WINDOWS_H_FOUND)

#find asm compiler
option (USE_YASM "Use YASM. If YASM is not enabled the assembly implementation will be disabled." ON)
if (USE_YASM)
  find_package (Yasm)
endif ()

if("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "armv7l")
  configure_file(platform/arm/config.h ${PROJECT_SOURCE_DIR}/config.h)
else()
  configure_file(platform/x86/config.h.in ${PROJECT_SOURCE_DIR}/config.h)
  configure_file(platform/x86/config.asm.in ${PROJECT_SOURCE_DIR}/config.asm)
endif()

if("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "armv7l")
	enable_language(ASM)
	add_definitions(
		-DEXTERN_ASM=
	)
endif()

if(WIN32)
    add_definitions(
        -Dsnprintf=avpriv_snprintf
        -Dvsnprintf=avpriv_vsnprintf
        -Dinline=__inline
        -Drestrict=__restrict
    )
endif()

#define asm sources
if(NOT ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "armv7l") )
if(YASM_FOUND)
set(YASM_NAMES
    libavutil/x86/cpuid.asm
    libavutil/x86/emms.asm
    libavcodec/x86/dct32.asm
    libavcodec/x86/deinterlace.asm
    libavcodec/x86/fft.asm
    libavcodec/x86/fpel.asm
    libavcodec/x86/h264_qpel_8bit.asm
    libavcodec/x86/h264_qpel_10bit.asm
    libavcodec/x86/hevc_mc.asm
    libavcodec/x86/hevc_deblock.asm
    libavcodec/x86/hevc_idct.asm
    libavcodec/x86/hevc_res_add.asm
    libavcodec/x86/hpeldsp.asm
    libavcodec/x86/qpel.asm
    libavcodec/x86/qpeldsp.asm
    libavcodec/x86/videodsp.asm
libavcodec/x86/h264_chromamc.asm
libavcodec/x86/h264_chromamc_10bit.asm
libavcodec/x86/h264_deblock.asm
libavcodec/x86/h264_deblock_10bit.asm
libavcodec/x86/h264_idct.asm
libavcodec/x86/h264_idct_10bit.asm
libavcodec/x86/h264_intrapred.asm
libavcodec/x86/h264_intrapred_10bit.asm
libavcodec/x86/h264_weight.asm
libavcodec/x86/h264_weight_10bit.asm
libavcodec/x86/idctdsp.asm
libavcodec/x86/simple_idct10.asm
libavcodec/x86/bswapdsp.asm
#libavcodec/x86/me_cmp.asm
#libavcodec/x86/xvididct.asm
)
endif(YASM_FOUND)
endif(NOT ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "armv7l") )

if(NOT ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "armv7l") )
set(COMMON_YASM_ARGS
    -I./
    -I "${CMAKE_CURRENT_SOURCE_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/config.asm"
    -I "${CMAKE_CURRENT_SOURCE_DIR}/libavfilter/x86/"
    -I "${CMAKE_CURRENT_SOURCE_DIR}/libavutil/x86/"
    -DPIC
)
if(CMAKE_BUILD_TYPE MATCHES "[Dd][Ee][Bb][Uu][Gg]")
set(COMMON_YASM_ARGS "${COMMON_YASM_ARGS} -g dwarf2")
endif(CMAKE_BUILD_TYPE MATCHES "[Dd][Ee][Bb][Uu][Gg]")
endif(NOT ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "armv7l") )

if(YASM_FOUND)
if(APPLE)
    set(YASM_ARGS -f macho64 -m amd64 -DPREFIX ${COMMON_YASM_ARGS})
elseif("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")
    set(YASM_ARGS -f elf     -m amd64 ${COMMON_YASM_ARGS})
elseif("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "i686")
    set(YASM_ARGS -f elf     -DX86_32 ${COMMON_YASM_ARGS})
else()
    set(YASM_ARGS -f win32   -m amd64 ${COMMON_YASM_ARGS})
endif()

#compile all asm files
foreach(_asm_file ${YASM_NAMES})
    set(YASM_SRC "${CMAKE_CURRENT_SOURCE_DIR}/${_asm_file}")
    get_filename_component(BASENAME ${YASM_SRC} NAME_WE)
    set(YASM_OBJ "${CMAKE_CURRENT_BINARY_DIR}/${BASENAME}.o")
    add_custom_command(
        OUTPUT ${YASM_OBJ}
        COMMAND "${YASM_EXECUTABLE}"
        ARGS ${YASM_ARGS} -o ${YASM_OBJ} ${YASM_SRC}
        DEPENDS ${YASM_SRC}
    )
    set(YASM_OBJECTS ${YASM_OBJECTS} ${YASM_OBJ})
endforeach()
endif(YASM_FOUND)

if("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")
    # Linker option: turning off symbol preemption
    if(MINGW OR NOT APPLE)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-Bsymbolic")
    endif()

    # Add Compiler flags for MINGW
    if(MINGW)
        AddCompilerFlag("-arch x86_64 -m64" C_FLAGS Vc_ARCHITECTURE_FLAGS)
    endif()

    # Add colored diagnostics 
    if(NOT("${CMAKE_COLOR_MAKEFILE}" STREQUAL "OFF"))
        if(MINGW)
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fdiagnostics-color")
        elseif(NOT APPLE)
            if ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
                execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
                if (GCC_VERSION VERSION_GREATER 4.9 OR GCC_VERSION VERSION_EQUAL 4.9)
                    set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -fdiagnostics-color")
                endif()
            endif()
        endif()
    endif()
elseif("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "i686")
    #add_definitions("-DX86_32")
endif()

add_definitions("-DPIC")
add_definitions("-DUSE_SDL")
AddCompilerFlag("-fpic" C_FLAGS Vc_ARCHITECTURE_FLAGS)
AddCompilerFlag("-fno-tree-vectorize" C_FLAGS Vc_ARCHITECTURE_FLAGS)

if(PROFILING)
  add_definitions("-pg")
  set(CMAKE_EXE_LINKER_FLAGS "-pg ${CMAKE_EXE_LINKER_FLAGS}")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${Vc_ARCHITECTURE_FLAGS}")
#find_package(CRYPTOPP_FOUND)
find_package(SDL)
find_package(SDL2)

set(libfilenames
# libavutil
    #libavutil/adler32.c
    libavutil/aes.c
    libavutil/aes_ctr.c
    #libavutil/audio_fifo.c
    libavutil/avstring.c
    libavutil/base64.c
    #libavutil/blowfish.c
    libavutil/bprint.c
    libavutil/buffer.c
    #libavutil/cast5.c
    #libavutil/camellia.c
    libavutil/channel_layout.c
    libavutil/cpu.c
    libavutil/crc.c
    libavutil/des.c
    libavutil/dict.c
    libavutil/display.c
    #libavutil/downmix_info.c
    libavutil/error.c
    libavutil/eval.c
    libavutil/fifo.c
    libavutil/file.c
    libavutil/file_open.c
    #libavutil/float_dsp.c
    #libavutil/fixed_dsp.c
    libavutil/frame.c
    #libavutil/hash.c
    libavutil/hmac.c
    #libavutil/hwcontext.c
    libavutil/imgutils.c
    #libavutil/integer.c
    libavutil/intmath.c
    libavutil/lfg.c
    #libavutil/lls.c
    libavutil/log.c
    libavutil/log2_tab.c
    libavutil/mathematics.c
    libavutil/mastering_display_metadata.c
    libavutil/md5.c
    libavutil/mem.c
    #libavutil/murmur3.c
    libavutil/opt.c
    libavutil/parseutils.c
    libavutil/pixdesc.c
    libavutil/random_seed.c
    libavutil/rational.c
    libavutil/reverse.c
    libavutil/rc4.c
    #libavutil/ripemd.c
    libavutil/samplefmt.c
    libavutil/sha.c
    libavutil/sha512.c
    libavutil/stereo3d.c
    #libavutil/threadmessage.c
    libavutil/time.c
    libavutil/timecode.c
    libavutil/tree.c
    #libavutil/twofish.c
    libavutil/utils.c
    #libavutil/xga_font_data.c
    #libavutil/xtea.c
    #libavutil/tea

    libavutil/atomic.c

# libavformat main files
    libavformat/allformats.c
    libavformat/avio.c
    libavformat/aviobuf.c
    libavformat/cutils.c
    #libavformat/dump.c
    libavformat/format.c
    libavformat/id3v1.c
    libavformat/id3v2.c
    libavformat/metadata.c
    #libavformat/mux.c
    libavformat/options.c
    #libavformat/os_support.c
    libavformat/qtpalette.c
    libavformat/protocols.c
    libavformat/riff.c
    #libavformat/sdp.c
    libavformat/url.c
    libavformat/utils.c
 
# libavformat subsystems    
    libavformat/isom.c
    libavformat/riffdec.c
   
# raw h264 and hevc demuxers
    libavformat/h264dec.c
    libavformat/hevcdec.c
    libavformat/rawdec.c

    # .mov .mp4 demuxers
    libavformat/mov.c
    libavformat/mov_chan.c
    libavformat/replaygain.c

    # MPEG-TS demuxer
    libavformat/mpegts.c

    #libavformat/cache.c
    #file protocol
    libavformat/file.c

# libavcodec main files
    libavcodec/allcodecs.c
    #libavcodec/audioconvert.c
    #libavcodec/avdct.c
    libavcodec/avpacket.c
    libavcodec/avpicture.c
    libavcodec/bitstream.c
    libavcodec/bitstream_filter.c
    libavcodec/bitstream_filters.c
    libavcodec/bsf.c  
    libavcodec/codec_desc.c
    #libavcodec/d3d11va.c
    #libavcodec/dirac.c
    #libavcodec/dv_profile.c
    #libavcodec/imgconvert.c
    #libavcodec/jni.c
    #libavcodec/mathtables.c
    #libavcodec/mediacodec.c
    #libavcodec/mpeg12framerate.c
    libavcodec/options.c
    libavcodec/parser.c
    libavcodec/profiles.c
    #libavcodec/qsv_api.c
    libavcodec/raw.c
    #libavcodec/resample.c
    #libavcodec/resample2.c
    libavcodec/utils.c
    #libavcodec/vorbis_parser.c
    libavcodec/xiph.c
    
# libavcodec sub systems
    libavcodec/bswapdsp.c
    libavcodec/cabac.c
    libavcodec/avfft.c
    libavcodec/dct.c
    libavcodec/dct32_float.c
    libavcodec/dct32_template.c
    libavcodec/faanidct.c
    libavcodec/fft_template.c
    libavcodec/golomb.c
    libavcodec/hpeldsp.c
    libavcodec/idctdsp.c
    libavcodec/simple_idct.c
    libavcodec/jrevdct.c
    libavcodec/mathtables.c
    libavcodec/qpeldsp.c
    libavcodec/rdft.c    
    libavcodec/startcode.c

# h264 decoder
    libavcodec/h264_cabac.c
    libavcodec/h264_cavlc.c
    libavcodec/h264_direct.c
    libavcodec/h264_loopfilter.c
    libavcodec/h264_mb.c
    #libavcodec/h264_mb_template.c
    #libavcodec/h264_mc_template.c
    libavcodec/h264_mvpred.h
    libavcodec/h264_parser.c
    libavcodec/h264_picture.c
    libavcodec/h264_ps.c
    libavcodec/h264_refs.c
    libavcodec/h264_sei.c
    libavcodec/h264_slice.c
    libavcodec/h264dec.c
    libavcodec/h264.h
    #libavcodec/h264chroma_template.c
    libavcodec/h264chroma.c
    libavcodec/h264chroma.h
    libavcodec/h264data.h
    #libavcodec/h264dsp_template.c
    libavcodec/h264dsp.c
    libavcodec/h264dsp.h
    #libavcodec/h264idct_template.c
    libavcodec/h264idct.c
    libavcodec/h264idct.h
    #libavcodec/h264pred_template.c
    libavcodec/h264pred.c
    libavcodec/h264pred.h
    #libavcodec/h264qpel_template.c
    libavcodec/h264qpel.c
    libavcodec/h264qpel.h
    libavcodec/h264_parse.c
    libavcodec/h264data.c
    libavcodec/h2645_parse.c

# hevc decoder
    libavcodec/hevc_cabac.c
    libavcodec/hevc_mvs.c
    libavcodec/hevc_parser.c
    libavcodec/hevc_ps.c
    libavcodec/hevc_refs.c
    libavcodec/hevc_sei.c
    libavcodec/hevc_filter.c
    libavcodec/hevc.c
    libavcodec/hevcdsp.c
    libavcodec/hevcpred.c

    libavcodec/pthread.c
    libavcodec/pthread_slice.c
    libavcodec/pthread_frame.c

# mov and isomedia decoder
    libavcodec/rawdec.c
    libavcodec/videodsp.c
    libavcodec/ac3tab.c
    libavcodec/mpegaudiodata.c
    libavcodec/mpeg4audio.c

# openHEVC wrapper
    gpac/modules/openhevc_dec/openHevcWrapper.c
)
if("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "armv7l")
list(APPEND libfilenames
    libavutil/arm/cpu.c
    libavutil/arm/asm.S
    libavcodec/arm/fft_init_arm.c
    libavcodec/arm/fft_neon.S
    libavcodec/arm/fft_vfp.S
    libavcodec/arm/hevcdsp_init_arm.c
    libavcodec/arm/hevcdsp_deblock_neon.S
    libavcodec/arm/hevcdsp_idct_neon.S
    libavcodec/arm/hevcdsp_init_arm.c
    libavcodec/arm/hevcdsp_qpel_neon.S
    libavcodec/arm/hevcdsp_epel_neon.S
    libavcodec/arm/hevcdsp_il_pred_neon.S
    libavcodec/arm/hevcdsp_sao_neon.S
    libavcodec/arm/hpeldsp_arm.S
    libavcodec/arm/hpeldsp_init_arm.c
    libavcodec/arm/hpeldsp_init_neon.c
    libavcodec/arm/hpeldsp_init_armv6.c
    libavcodec/arm/hpeldsp_neon.S
    libavcodec/arm/hpeldsp_armv6.S
    libavcodec/arm/int_neon.S
    #libavcodec/arm/jrevdct_arm.S
    libavcodec/arm/mdct_neon.S
    libavcodec/arm/mdct_vfp.S
    libavcodec/arm/rdft_neon.S
    libavcodec/arm/simple_idct_arm.S
    libavcodec/arm/simple_idct_armv6.S
    libavcodec/arm/simple_idct_neon.S
    libavcodec/arm/videodsp_init_arm.c
)
else()
list(APPEND libfilenames
    libavutil/x86/cpu.c
    libavcodec/x86/constants.c
    libavcodec/x86/dct_init.c
    libavcodec/x86/fdct.c
    libavcodec/x86/fft_init.c
    libavcodec/x86/h264_qpel.c
    libavcodec/x86/hevcdsp_init.c
    libavcodec/x86/hevcpred_init.c
    libavcodec/x86/hevc_idct_sse.c
    libavcodec/x86/hevc_il_pred_sse.c
    libavcodec/x86/hevc_mc_sse.c
    libavcodec/x86/hevc_mc_avx2.c
    libavcodec/x86/hevc_sao_sse.c
    libavcodec/x86/hevc_intra_pred_sse.c
    libavcodec/x86/hpeldsp_init.c
    libavcodec/x86/idct_mmx_xvid.c
    libavcodec/x86/idct_sse2_xvid.c
    libavcodec/x86/qpeldsp_init.c
    libavcodec/x86/simple_idct.c
    libavcodec/x86/videodsp_init.c
    libavcodec/x86/h264dsp_init.c
    libavcodec/x86/h264_intrapred_init.c
    libavcodec/x86/bswapdsp_init.c
    #libavcodec/x86/me_cmp_init.c
    libavcodec/x86/idctdsp_init.c
    libavcodec/x86/h264chroma_init.c
    #libavcodec/x86/xvididct_init.c
)
endif()

if(WIN32)
list(APPEND libfilenames
    compat/strtod.c
    compat/msvcrt/snprintf.c
)
endif()

message (STATUS "CRYPTOPP_FOUND  = ${CRYPTOPP_FOUND}")

if(CRYPTOPP_FOUND)
list(APPEND libfilenames
    libavcodec/crypto.cpp
)
endif()

if (ENABLE_STATIC)
    option (BUILD_SHARED_LIBS "enabled static library instead of shared" OFF)
else()
    option (BUILD_SHARED_LIBS "enabled static library instead of shared" ON)
endif()
add_library (LibOpenHevcWrapper ${libfilenames} ${YASM_OBJECTS})


if(CRYPTOPP_FOUND)
target_link_libraries(LibOpenHevcWrapper cryptopp)
else()
target_link_libraries(LibOpenHevcWrapper m)
endif()

include_directories(. gpac/modules/openhevc_dec/ platform/x86/)

option(ENABLE_EXECUTABLE "Generate the test application" ON)

if(ENABLE_EXECUTABLE)

    set(LINK_LIBRARIES_LIST LibOpenHevcWrapper)
    set(HEVC_SOURCES_FILES
        main_hm/getopt.c
        main_hm/main.c
    )
    if(MINGW)
        list(APPEND LINK_LIBRARIES_LIST -lwinmm)
    endif()

    if(SDL_FOUND)
        # Build executable
        add_executable(hevc ${HEVC_SOURCES_FILES} main_hm/sdl.c main_hm/SDL_framerate.c)
        # Link executable
        target_link_libraries(hevc ${LINK_LIBRARIES_LIST} ${SDL_LIBRARY})
        # Set include directory specific for this file. Avoid conflicts when including SDL.h
        # if both SDL and SDL2 are installed
        set_source_files_properties(main_hm/sdl.c main_hm/SDL_framerate.c PROPERTIES COMPILE_FLAGS -I"${SDL_INCLUDE_DIR}")
    endif()

    if(SDL2_FOUND)
        # Build executable
        add_executable(hevc_sdl2 ${HEVC_SOURCES_FILES} main_hm/sdl2.c main_hm/SDL_framerate.c)
        # Link executable
        target_link_libraries(hevc_sdl2 ${LINK_LIBRARIES_LIST} ${SDL2_LIBRARY})
        # Set include directory specific for this file. Avoid conflicts when including SDL.h
        # if both SDL and SDL2 are installed
        set_source_files_properties(main_hm/sdl2.c main_hm/SDL_framerate.c PROPERTIES COMPILE_FLAGS -I"${SDL2_INCLUDE_DIR}")
    endif()

endif()

install(FILES
    gpac/modules/openhevc_dec/openHevcWrapper.h
    libavcodec/hevcdsp.h
    libavcodec/hevcpred.h
    DESTINATION include
)

install(
    TARGETS LibOpenHevcWrapper
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/uninstall_target.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/uninstall_target.cmake"
    IMMEDIATE @ONLY)

# uninstall target
add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/uninstall_target.cmake)
