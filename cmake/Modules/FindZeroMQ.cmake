set(PKG_CONFIG_USE_CMAKE_PREFIX_PATH ON)
find_package(PkgConfig)
pkg_check_modules(PC_LIBZMQ QUIET libzmq)

set(ZeroMQ_VERSION ${PC_LIBZMQ_VERSION})
find_path(ZeroMQ_INCLUDE_DIR zmq.h PATHS ${PC_LIBZMQ_INCLUDE_DIRS} $ENV{ZMQDIR})

find_library(ZeroMQ_LIBRARY NAMES libzmq.so libzmq.dylib libzmq.dll
PATHS ${PC_LIBZMQ_LIBDIR} ${PC_LIBZMQ_LIBRARY_DIRS} $ENV{ZMQDIR})
find_library(ZeroMQ_STATIC_LIBRARY NAMES libzmq-static.a libzmq.a libzmq.dll.a
             PATHS ${PC_LIBZMQ_LIBDIR} ${PC_LIBZMQ_LIBRARY_DIRS} $ENV{ZMQDIR})

if(ZeroMQ_LIBRARY OR ZeroMQ_STATIC_LIBRARY)
    set(ZeroMQ_FOUND ON)
endif()

if (TARGET libzmq)
    # avoid errors defining targets twice
    return()
endif()

set(ZeroMQ_INCLUDE_DIRS ${ZeroMQ_INCLUDE_DIR})
list(APPEND ZeroMQ_INCLUDE_DIRS ${PC_LIBZMQ_INCLUDE_DIRS})
list(REMOVE_DUPLICATES ZeroMQ_INCLUDE_DIRS)

add_library(libzmq SHARED IMPORTED)
set_property(TARGET libzmq PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${ZeroMQ_INCLUDE_DIRS})
set_property(TARGET libzmq PROPERTY IMPORTED_LOCATION ${ZeroMQ_LIBRARY})

add_library(libzmq-static STATIC IMPORTED ${ZeroMQ_INCLUDE_DIRS})
set_property(TARGET libzmq-static PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${ZeroMQ_INCLUDE_DIRS})
set_property(TARGET libzmq-static PROPERTY IMPORTED_LOCATION ${ZeroMQ_STATIC_LIBRARY})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ZeroMQ
  REQUIRED_VARS
    ZeroMQ_LIBRARY ZeroMQ_INCLUDE_DIRS
  VERSION_VAR
    ZeroMQ_VERSION)
