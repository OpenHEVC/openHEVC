cmake_minimum_required (VERSION 2.6)

# Common packages required
project (HM-8.0-dec)

set(HM-8.0-INCLUDE_DIR source/Lib)

include_directories(${HM-8.0-INCLUDE_DIR})

file ( GLOB libmd5_source_files Source/Lib/libmd5/*.c Source/Lib/libmd5/*.h)
add_library (libmd5 STATIC ${libmd5_source_files})

file ( GLOB TLibCommon_source_files Source/Lib/TLibCommon/*.cpp Source/Lib/TLibCommon/*.h)
add_library (TLibCommon STATIC ${TLibCommon_source_files})

file ( GLOB TLibDecoder_source_files Source/Lib/TLibDecoder/*.cpp Source/Lib/TLibDecoder/*.h)
add_library (TLibDecoder STATIC ${TLibDecoder_source_files})

file ( GLOB TLibEncoder_source_files Source/Lib/TLibEncoder/*.cpp Source/Lib/TLibEncoder/*.h)
add_library (TLibEncoder STATIC ${TLibEncoder_source_files})

file ( GLOB TLibVideoIO_source_files Source/Lib/TLibVideoIO/*.cpp Source/Lib/TLibVideoIO/*.h)
add_library (TLibVideoIO STATIC ${TLibVideoIO_source_files})

file ( GLOB TAppCommon_source_files Source/Lib/TAppCommon/*.cpp Source/Lib/TAppCommon/*.h)
add_library (TAppCommon STATIC ${TAppCommon_source_files})

file ( GLOB TAppDecoder_source_files Source/App/TAppDecoder/*.cpp Source/App/TAppDecoder/*.h)
add_executable(TAppDecoder ${TAppDecoder_source_files})
target_link_libraries(TAppDecoder TLibDecoder TLibVideoIO TLibCommon libmd5 TAppCommon)

file ( GLOB TAppEncoder_source_files Source/App/TAppEncoder/*.cpp Source/App/TAppEncoder/*.h)
add_executable(TAppEncoder ${TAppEncoder_source_files})
target_link_libraries(TAppEncoder TLibEncoder TLibVideoIO TLibCommon libmd5 TAppCommon)
