cmake_minimum_required(VERSION 3.16)

include(ExternalProject)

set(compilers_override CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER} CXXFLAGS="-Og")
set(superbuild_prefix ${CMAKE_INSTALL_PREFIX})
set(libdir lib)
set(libdir_abs_path ${superbuild_prefix}/${libdir})
set(pkg_config_path ${libdir_abs_path}/pkgconfig)

find_program(make_cmd NAMES gmake make mingw32-make REQUIRED)

ExternalProject_Add(external_x264  
  GIT_REPOSITORY https://code.videolan.org/videolan/x264.git
  GIT_TAG 4613ac3c15fd75cebc4b9f65b7fb95e70a3acce1
  UPDATE_DISCONNECTED true
  CONFIGURE_HANDLED_BY_BUILD true
  CONFIGURE_COMMAND  
  ${CMAKE_COMMAND} -E env ${compilers_override} <SOURCE_DIR>/configure --prefix=${superbuild_prefix} --enable-debug
  BUILD_COMMAND
  ${make_cmd} -j4
  INSTALL_COMMAND
  ${make_cmd} install-lib-static
)
set(libx264_libname ${CMAKE_STATIC_LIBRARY_PREFIX}x264${CMAKE_STATIC_LIBRARY_SUFFIX})
add_library(libx264 INTERFACE)
add_dependencies(libx264 external_x264)
target_link_directories(libx264 INTERFACE ${libdir_abs_path})
target_link_libraries(libx264 INTERFACE ${libx264_libname})
target_include_directories(libx264 INTERFACE ${superbuild_prefix}/include/)
add_library(libx264::libx264 ALIAS libx264)
