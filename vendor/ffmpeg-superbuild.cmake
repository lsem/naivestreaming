cmake_minimum_required(VERSION 3.16)

include(ExternalProject)


###################################################################
# Taken from system (not a superbuild concept)
##################################################################
find_package(LibLZMA)
find_package(ZLIB)
find_package(PkgConfig REQUIRED)

include(CMakePrintHelpers)
pkg_check_modules(LibDRM REQUIRED libdrm)
cmake_print_variables(LibDRM_INCLUDE_DIRS LibDRM_LIBRARY_DIRS LibDRM_LIBRARIES)
add_library(libdrm INTERFACE)
target_include_directories(libdrm INTERFACE ${LibDRM_INCLUDE_DIRS})
target_link_directories(libdrm INTERFACE ${LibDRM_LIBRARY_DIRS})
target_link_libraries(libdrm INTERFACE ${LibDRM_LIBRARIES})
add_library(libdrm::libdrm ALIAS libdrm)

##################################################################

set(compilers_override CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER})
set(superbuild_prefix ${CMAKE_INSTALL_PREFIX})
message(STATUS "SUPERBUILD_PREFIX: ${superbuild_prefix}")
set(libdir lib)
set(libdir_abs_path ${superbuild_prefix}/${libdir})
set(pkg_config_path ${libdir_abs_path}/pkgconfig)

ExternalProject_Add(external_ffmpeg
  GIT_REPOSITORY https://git.ffmpeg.org/ffmpeg.git
  GIT_TAG 53d0f9afb46ac811269252c9e3be000fc7c3b2cc
  UPDATE_DISCONNECTED true
  CONFIGURE_HANDLED_BY_BUILD true
  CONFIGURE_COMMAND  
  ${CMAKE_COMMAND} -E env ${compilers_override} <SOURCE_DIR>/configure --prefix=${superbuild_prefix} --disable-programs --disable-ffplay --disable-ffprobe --disable-doc --disable-htmlpages --disable-manpages --disable-podpages --disable-txtpages
  BUILD_COMMAND
  ${make_cmd} -j4
  INSTALL_COMMAND
  ${make_cmd} install
)

set(ffmpeg_libavcodec_libname ${CMAKE_STATIC_LIBRARY_PREFIX}avcodec${CMAKE_STATIC_LIBRARY_SUFFIX})
set(ffmpeg_libswresample_libname ${CMAKE_STATIC_LIBRARY_PREFIX}swresample${CMAKE_STATIC_LIBRARY_SUFFIX})
set(ffmpeg_libavutils_libname ${CMAKE_STATIC_LIBRARY_PREFIX}avutil${CMAKE_STATIC_LIBRARY_SUFFIX})
set(ffmpeg_libavformat_libname ${CMAKE_STATIC_LIBRARY_PREFIX}avformat${CMAKE_STATIC_LIBRARY_SUFFIX})
add_library(libffmpeg INTERFACE)
add_dependencies(libffmpeg external_ffmpeg)
target_link_directories(libffmpeg INTERFACE ${superbuild_prefix}/lib)
target_link_libraries(libffmpeg INTERFACE ${ffmpeg_libavcodec_libname} ${ffmpeg_libswresample_libname} ${ffmpeg_libavutils_libname} ${ffmpeg_libavutils_libname} ${ffmpeg_libavformat_libname} LibLZMA::LibLZMA ZLIB::ZLIB libdrm::libdrm)
target_include_directories(libffmpeg INTERFACE ${superbuild_prefix}/include/)
add_library(ffmpeg::avfamily ALIAS libffmpeg)
