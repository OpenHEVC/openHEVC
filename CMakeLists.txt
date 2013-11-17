cmake_minimum_required (VERSION 2.8)
project (libavHM)
SET(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/MyCMakeScripts)

message("DEBUG: CMAKE_SYSTEM_PROCESSOR = ${CMAKE_SYSTEM_PROCESSOR}")
message("DEBUG: CMAKE_SYSTEM_NAME = ${CMAKE_SYSTEM_NAME}")

#find asm compiler
FIND_PROGRAM(YASM_EXECUTABLE yasm)
#define asm sources
SET(YASM_NAMES )

set(YASM_NAMES
libavcodec/x86/hevc_deblock
libavutil/x86/cpuid
)

if(APPLE)
SET(YASM_ARGS -f macho64 -m amd64 -DPIC -DPREFIX -I ${CMAKE_CURRENT_SOURCE_DIR} -I./ -P${CMAKE_CURRENT_SOURCE_DIR}/config.asm -I ${CMAKE_CURRENT_SOURCE_DIR}/libavfilter/x86/ -I ${CMAKE_CURRENT_SOURCE_DIR}/libavutil/x86/)
else(APPLE)
IF("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")
 SET(YASM_ARGS -f elf     -m amd64 -DPIC -I ${CMAKE_CURRENT_SOURCE_DIR} -I./ -P${CMAKE_CURRENT_SOURCE_DIR}/config.asm -I ${CMAKE_CURRENT_SOURCE_DIR}/libavfilter/x86/ -I ${CMAKE_CURRENT_SOURCE_DIR}/libavutil/x86/)
ELSE("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")
IF("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "i686")
SET(YASM_ARGS -f elf    -DX86_32 -DPIC -I ${CMAKE_CURRENT_SOURCE_DIR} -I./ -P${CMAKE_CURRENT_SOURCE_DIR}/config.asm -I ${CMAKE_CURRENT_SOURCE_DIR}/libavfilter/x86/ -I ${CMAKE_CURRENT_SOURCE_DIR}/libavutil/x86/)
ELSE("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "i686")
SET(YASM_ARGS -f win32    -m amd64 -DPIC -I ${CMAKE_CURRENT_SOURCE_DIR} -I./ -P${CMAKE_CURRENT_SOURCE_DIR}/config.asm -I ${CMAKE_CURRENT_SOURCE_DIR}/libavfilter/x86/ -I ${CMAKE_CURRENT_SOURCE_DIR}/libavutil/x86/)
ENDIF("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "i686")
ENDIF("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")
endif(APPLE)

#compile all asm files
FOREACH(src ${YASM_NAMES})
      SET(YASM_SRC ${CMAKE_CURRENT_SOURCE_DIR}/${src}.asm)
      SET(YASM_OBJ ${CMAKE_CURRENT_SOURCE_DIR}/${src}.o)
      ADD_CUSTOM_COMMAND(
      OUTPUT ${YASM_OBJ}
      COMMAND ${YASM_EXECUTABLE}     
               ARGS ${YASM_ARGS} -o ${YASM_OBJ} ${YASM_SRC}
      DEPENDS ${YASM_SRC}
      )
      SET(YASM_OBJECTS ${YASM_OBJECTS} ${YASM_OBJ})
ENDFOREACH(src)

if(NOT ((CMAKE_BUILD_TYPE STREQUAL "DEBUG") OR (CMAKE_BUILD_TYPE STREQUAL "RELEASE")))
    message(FATAL_ERROR "Set CMAKE_BUILD_TYPE to DEBUG or RELEASE.\nFor example: cmake -DCMAKE_BUILD_TYPE=RELEASE")
endif(NOT ((CMAKE_BUILD_TYPE STREQUAL "DEBUG") OR (CMAKE_BUILD_TYPE STREQUAL "RELEASE")))


IF("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")
#SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -arch x86_64 -m64  -DPIC -Wl,-Bsymbolic -fno-tree-vectorize -mssse3 -msse4 -msse4.1")
#SET(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG}   -arch x86_64 -m64  -DPIC -Wl,-Bsymbolic -fno-tree-vectorize -mssse3 -msse4 -msse4.1")
if(MINGW)
SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -arch x86_64 -m64  -DPIC -Wl,-Bsymbolic -mssse3 -msse4 -msse4.1 -fno-tree-vectorize")
SET(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG}   -arch x86_64 -m64  -DPIC -Wl,-Bsymbolic -mssse3 -msse4 -msse4.1 -fno-tree-vectorize")
else(MINGW)
SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -DPIC -fPIC -Wl,-Bsymbolic -mssse3 -msse4 -msse4.1 -fno-tree-vectorize")
SET(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG}   -DPIC -fPIC -Wl,-Bsymbolic -mssse3 -msse4 -msse4.1 -fno-tree-vectorize")
endif(MINGW)
ELSE("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_32")
if(APPLE)
SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -D_ISOC99_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -DHAVE_AV_CONFIG_H -DPIC -fPIC -mssse3 -msse4 -msse4.1"  )
SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_ISOC99_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -DHAVE_AV_CONFIG_H -DPIC -fPIC -mssse3 -msse4 -msse4.1"  )
SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -D_ISOC99_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -DHAVE_AV_CONFIG_H -DPIC -fPIC -mssse3 -msse4 -msse4.1"  )
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -D_ISOC99_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -DHAVE_AV_CONFIG_H -DPIC -fPIC -mssse3 -msse4 -msse4.1"  )
else(APPLE)
IF("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "i686")
SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -DX86_32 -DPIC -fPIC -Wl,-Bsymbolic -mssse3 -msse4 -msse4.1 -fno-tree-vectorize")
SET(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG}   -DX86_32 -DPIC -fPIC -Wl,-Bsymbolic -mssse3 -msse4 -msse4.1 -fno-tree-vectorize")
else("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "i686")
SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -DPIC -fPIC -Wl,-Bsymbolic -mssse3 -msse4 -msse4.1 -fno-tree-vectorize")
SET(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG}   -DPIC -fPIC -Wl,-Bsymbolic -mssse3 -msse4 -msse4.1 -fno-tree-vectorize")
endif("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "i686")
endif(APPLE)
ENDIF("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")



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
libavcodec/x86/hevc_dequant_sse4.c
libavcodec/x86/hevc_idct_sse4.c
libavcodec/x86/hevc_mc_sse.c
libavcodec/x86/hevc_il_pred_sse.c
libavcodec/x86/hevc_sao_sse.c
libavcodec/x86/hevc_intra_pred_sse.c
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
libavcodec/pthread.c
libavcodec/simple_idct.c
libavcodec/utils.c
libavcodec/videodsp.c
)

add_subdirectory(wrapper_hm)

OPTION(ENABLE_STATIC "enabled static library instead of shared" OFF)
if (ENABLE_STATIC)
add_library (LibOpenHevcWrapper STATIC ${libfilenames} ${YASM_OBJECTS})
else(ENABLE_STATIC)
add_library (LibOpenHevcWrapper SHARED ${libfilenames} ${YASM_OBJECTS})
endif(ENABLE_STATIC)
include_directories(. gpac/modules/openhevc_dec/)

OPTION(ENABLE_EXECUTABLE "Generate the test application" ON)

if (ENABLE_EXECUTABLE)
if(SDL_FOUND)
set_source_files_properties(main_hm/sdl.c PROPERTIES COMPILE_FLAGS -I"${SDL_INCLUDE_DIR}")
add_executable(hevc main_hm/getopt.c main_hm/main.c main_hm/sdl.c)
if(MINGW)
target_link_libraries(hevc LibOpenHevcWrapper ${SDL_LIBRARY} -lwinmm)
else(MINGW)
target_link_libraries(hevc LibOpenHevcWrapper ${SDL_LIBRARY})
endif(MINGW)
endif()

if(SDL2_FOUND)
set_source_files_properties(main_hm/sdl2.c PROPERTIES COMPILE_FLAGS -I"${SDL2_INCLUDE_DIR}")
add_executable(hevc_sdl2 main_hm/getopt.c main_hm/main.c main_hm/sdl2.c)
if(MINGW)
target_link_libraries(hevc_sdl2 LibOpenHevcWrapper ${SDL2_LIBRARY} -lwinmm)
else(MINGW)
target_link_libraries(hevc_sdl2 LibOpenHevcWrapper ${SDL2_LIBRARY} )
endif(MINGW)
endif()
endif(ENABLE_EXECUTABLE)

INSTALL(FILES "gpac/modules/openhevc_dec/openHevcWrapper.h" DESTINATION include)

INSTALL(TARGETS LibOpenHevcWrapper DESTINATION lib PERMISSIONS
OWNER_READ OWNER_WRITE OWNER_EXECUTE
GROUP_READ GROUP_EXECUTE
WORLD_READ WORLD_EXECUTE)
