cmake_minimum_required (VERSION 2.8)
project (openHEVC)
SET(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/MyCMakeScripts)

message("DEBUG: CMAKE_SYSTEM_PROCESSOR = ${CMAKE_SYSTEM_PROCESSOR}")
message("DEBUG: CMAKE_SYSTEM_NAME = ${CMAKE_SYSTEM_NAME}")

# Configure build type (Release when no one is selected)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
    set(BUILDTYPE_MSG "Release (set by default)")
else()
    set(BUILDTYPE_MSG "${CMAKE_BUILD_TYPE}")
endif()

#find asm compiler
find_program(YASM_EXECUTABLE yasm)

#define asm sources
set(YASM_NAMES
    libavcodec/x86/hevc_mc.asm
    libavcodec/x86/hevc_deblock.asm
    libavutil/x86/cpuid.asm
    libavcodec/x86/videodsp.asm
)

set(COMMON_YASM_ARGS
    -I./
    -I "${CMAKE_CURRENT_SOURCE_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/config.asm"
    -I "${CMAKE_CURRENT_SOURCE_DIR}/libavfilter/x86/"
    -I "${CMAKE_CURRENT_SOURCE_DIR}/libavutil/x86/"
    -DPIC
)

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

if("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")
    if(MINGW)
        set(CMAKE_C_FLAGS "-arch x86_64 -m64 -fPIC -DPIC -Wl,-Bsymbolic -march=native -fno-tree-vectorize")
    else()
        set(CMAKE_C_FLAGS "-DPIC -fPIC -Wl,-Bsymbolic -march=native -fno-tree-vectorize")
    endif()
elseif(APPLE)
    set(CMAKE_C_FLAGS   "-D_ISOC99_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -DHAVE_AV_CONFIG_H -DPIC -fPIC -mssse3 -msse4 -msse4.1")
elseif("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "i686")
    set(CMAKE_C_FLAGS "-DX86_32 -DPIC -fPIC -Wl,-Bsymbolic -march=native -fno-tree-vectorize")
endif()

find_package(SDL)
find_package(SDL2)

set(libfilenames
    libavutil/audioconvert.c
    libavutil/avstring.c
    libavutil/atomic.c
    libavutil/base64.c
    libavutil/buffer.c
    libavutil/cpu.c
    libavutil/dict.c
    libavutil/eval.c
    libavutil/frame.c
    libavutil/imgutils.c
    libavutil/log.c
    libavutil/mathematics.c
    libavutil/crc.c
    libavutil/md5.c
    libavutil/mem.c
    libavutil/opt.c
    libavutil/parseutils.c
    libavutil/pixdesc.c
    libavutil/rational.c
    libavutil/random_seed.c
    libavutil/samplefmt.c
    libavutil/time.c
    libavutil/x86/cpu.c
    gpac/modules/openhevc_dec/openHevcWrapper.c
    libavformat/allformats.c
    libavformat/avc.c
    libavformat/avio.c
    libavformat/aviobuf.c
    libavformat/concat.c
    libavformat/cutils.c
    libavformat/file.c
    libavformat/format.c
    libavformat/id3v2.c
    libavformat/isom.c
    libavformat/hevcdec.c
    libavformat/matroska.c
    libavformat/matroskadec.c
    libavformat/metadata.c
    libavformat/mov.c
    libavformat/movenc.c
    libavformat/movenchint.c
    libavformat/mpegts.c
    libavformat/options.c
    libavformat/os_support.c
    libavformat/rawdec.c
    libavformat/utils.c
    libavformat/url.c
    libavformat/urldecode.c
    libavcodec/x86/hevcdsp_init.c
    libavcodec/x86/hevcpred_init.c
    libavcodec/x86/hevc_idct_sse4.c
    libavcodec/x86/hevc_il_pred_sse.c
    libavcodec/x86/hevc_mc_sse.c
    libavcodec/x86/hevc_sao_sse.c
    libavcodec/x86/hevc_intra_pred_sse.c
    libavcodec/x86/videodsp_init.c
    libavcodec/allcodecs.c
    libavcodec/avpacket.c
    libavcodec/avpicture.c
    libavcodec/cabac.c
    libavcodec/dsputil.c
    libavcodec/golomb.c
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
    libavcodec/imgconvert.c
    libavcodec/options.c
    libavcodec/parser.c
    libavcodec/pthread_slice.c
    libavcodec/pthread_frame.c
    libavcodec/pthread.c
    libavcodec/simple_idct.c
    libavcodec/utils.c
    libavcodec/videodsp.c
)

option(ENABLE_STATIC "enabled static library instead of shared" OFF)

if(ENABLE_STATIC)
    add_library (LibOpenHevcWrapper STATIC ${libfilenames} ${YASM_OBJECTS})
else()
    add_library (LibOpenHevcWrapper SHARED ${libfilenames} ${YASM_OBJECTS})
endif()

include_directories(. gpac/modules/openhevc_dec/)

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
        add_executable(hevc ${HEVC_SOURCES_FILES} main_hm/sdl.c)
        # Link executable
        target_link_libraries(hevc ${LINK_LIBRARIES_LIST} ${SDL_LIBRARY})
        # Set include directory specific for this file. Avoid conflicts when including SDL.h
        # if both SDL and SDL2 are installed
        set_source_files_properties(main_hm/sdl.c PROPERTIES COMPILE_FLAGS -I"${SDL_INCLUDE_DIR}")
    endif()

    if(SDL2_FOUND)
        # Build executable
        add_executable(hevc_sdl2 ${HEVC_SOURCES_FILES} main_hm/sdl2.c)
        # Link executable
        target_link_libraries(hevc_sdl2 ${LINK_LIBRARIES_LIST} ${SDL2_LIBRARY})
        # Set include directory specific for this file. Avoid conflicts when including SDL.h
        # if both SDL and SDL2 are installed
        set_source_files_properties(main_hm/sdl2.c PROPERTIES COMPILE_FLAGS -I"${SDL2_INCLUDE_DIR}")
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
    DESTINATION lib
    PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE
)

# uninstall target
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/uninstall_target.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/uninstall_target.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/uninstall_target.cmake)
