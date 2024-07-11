include(CMakePrintHelpers)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LibAvFormat REQUIRED libavformat libavcodec libavutil)
# list(TRANSFORM LibAvFormat_LIBRARIES PREPEND ${CMAKE_STATIC_LIBRARY_PREFIX})
# list(TRANSFORM LibAvFormat_LIBRARIES APPEND ${CMAKE_STATIC_LIBRARY_SUFFIX})
list(TRANSFORM LibAvFormat_LIBRARIES PREPEND ${CMAKE_SHARED_LIBRARY_PREFIX})
list(TRANSFORM LibAvFormat_LIBRARIES APPEND ${CMAKE_SHARED_LIBRARY_SUFFIX})

cmake_print_variables(LibAvFormat_INCLUDE_DIRS LibAvFormat_LIBRARY_DIRS LibAvFormat_LIBRARIES)

add_library(avfamily INTERFACE)
target_include_directories(avfamily INTERFACE ${LibAvFormat_INCLUDE_DIRS})
target_link_directories(avfamily INTERFACE ${LibAvFormat_LIBRARY_DIRS})
target_link_libraries(avfamily INTERFACE ${LibAvFormat_LIBRARIES})
add_library(ffmpeg::avfamily ALIAS avfamily)


