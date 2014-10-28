# vim:set ts=2 sw=2 sts=2 et:
cmake_minimum_required(VERSION 2.8)
project(rabbitmq-c "CXX")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Follow all steps below in order to calculate new ABI version when updating the library
# NOTE: THIS IS UNRELATED to the actual project version
#
# 1. If the library source code has changed at all since the last update, then increment revision
# 2. If any interfaces have been added, removed, or changed since the last update, increment current and set revision to 0.
# 3. If any interfaces have been added since the last public release, then increment age.
# 4. If any interfaces have been removed since the last public release, then set age to 0.

set(RMQ2_SOVERSION_CURRENT   0)
set(RMQ2_SOVERSION_REVISION  0)
set(RMQ2_SOVERSION_AGE       0)

math(EXPR RMQ2_SOVERSION_MAJOR "${RMQ2_SOVERSION_CURRENT} - ${RMQ2_SOVERSION_AGE}")
math(EXPR RMQ2_SOVERSION_MINOR "${RMQ2_SOVERSION_AGE}")
math(EXPR RMQ2_SOVERSION_PATCH "${RMQ2_SOVERSION_REVISION}")

set(RMQ2_VERSION ${RMQ2_SOVERSION_MAJOR}.${RMQ2_SOVERSION_MINOR}.${RMQ2_SOVERSION_PATCH})
set(RMQ2_SOVERSION ${RMQ2_SOVERSION_MAJOR})

file(STRINGS rmq2/version.h _API_VERSION_MAJOR REGEX "^#define RMQ2_VERSION_MAJOR [0-9]+$")
file(STRINGS rmq2/version.h _API_VERSION_MINOR REGEX "^#define RMQ2_VERSION_MINOR [0-9]+$")
file(STRINGS rmq2/version.h _API_VERSION_PATCH REGEX "^#define RMQ2_VERSION_PATCH [0-9]+$")

string(REGEX MATCH "[0-9]+" _API_VERSION_MAJOR ${_API_VERSION_MAJOR})
string(REGEX MATCH "[0-9]+" _API_VERSION_MINOR ${_API_VERSION_MINOR})
string(REGEX MATCH "[0-9]+" _API_VERSION_PATCH ${_API_VERSION_PATCH})

set(RMQ2_API_VERSION ${_API_VERSION_MAJOR}.${_API_VERSION_MINOR}.${_API_VERSION_PATCH})

if (MSVC)
  message(INFO "MSVC support isn't enabled yet.")
elseif (CMAKE_C_COMPILER_ID MATCHES ".*Clang")
  set(CMAKE_CXX_FLAGS "-Wall -Wextra -std=c++11 -fvisibility=hidden")
elseif (CMAKE_COMPILER_IS_GNUCC)
  set(CMAKE_CXX_FLAGS "-Wall -Wextra -std=c++11 -fvisibility=hidden")
endif ()

if (CMAKE_GENERATOR MATCHES ".*(Make|Ninja).*"
    AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
  message(STATUS "CMAKE_BUILD_TYPE not specified. Creating ${CMAKE_BUILD_TYPE} build")
endif()

include(CheckFunctionExists)
include(CheckSymbolExists)
include(CheckLibraryExists)
include(CMakePushCheckState)
include(GNUInstallDirs)


option(REGENERATE_AMQP_FRAMING "Regenerate amqp_framing.h/amqp_framing.c sources (for developer use)" OFF)
mark_as_advanced(REGENERATE_AMQP_FRAMING)

if (REGENERATE_AMQP_FRAMING)
  find_package(PythonInterp)
  if (NOT PYTHONINTERP_FOUND)
    message(FATAL_ERROR "REGENERATE_AMQP_FRAMING requires Python to be available")
  endif ()

  #Determine Python Version:
  if(PYTHON_EXECUTABLE)
    execute_process(COMMAND "${PYTHON_EXECUTABLE}" -c
      "import sys; sys.stdout.write(';'.join([str(x) for x in sys.version_info[:3]]))"
      OUTPUT_VARIABLE _VERSION
      RESULT_VARIABLE _PYTHON_VERSION_RESULT
      ERROR_QUIET)
    if(NOT _PYTHON_VERSION_RESULT)
      string(REPLACE ";" "." PYTHON_VERSION_STRING "${_VERSION}")
      list(GET _VERSION 0 PYTHON_VERSION_MAJOR)
      list(GET _VERSION 1 PYTHON_VERSION_MINOR)
      list(GET _VERSION 2 PYTHON_VERSION_PATCH)
      if(PYTHON_VERSION_PATCH EQUAL 0)
        # it's called "Python 2.7", not "2.7.0"
        string(REGEX REPLACE "\\.0$" "" PYTHON_VERSION_STRING "${PYTHON_VERSION_STRING}")
      endif()
    else()
      # sys.version predates sys.version_info, so use that
      execute_process(COMMAND "${PYTHON_EXECUTABLE}" -c "import sys; sys.stdout.write(sys.version)"
        OUTPUT_VARIABLE _VERSION
        RESULT_VARIABLE _PYTHON_VERSION_RESULT
        ERROR_QUIET)
      if(NOT _PYTHON_VERSION_RESULT)
        string(REGEX REPLACE " .*" "" PYTHON_VERSION_STRING "${_VERSION}")
        string(REGEX REPLACE "^([0-9]+)\\.[0-9]+.*" "\\1" PYTHON_VERSION_MAJOR "${PYTHON_VERSION_STRING}")
        string(REGEX REPLACE "^[0-9]+\\.([0-9])+.*" "\\1" PYTHON_VERSION_MINOR "${PYTHON_VERSION_STRING}")
        if(PYTHON_VERSION_STRING MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+.*")
          string(REGEX REPLACE "^[0-9]+\\.[0-9]+\\.([0-9]+).*" "\\1" PYTHON_VERSION_PATCH "${PYTHON_VERSION_STRING}")
        else()
          set(PYTHON_VERSION_PATCH "0")
        endif()
      else()
        # sys.version was first documented for Python 1.5, so assume
        # this is older.
        set(PYTHON_VERSION_STRING "1.4")
        set(PYTHON_VERSION_MAJOR "1")
        set(PYTHON_VERSION_MAJOR "4")
        set(PYTHON_VERSION_MAJOR "0")
      endif()
    endif()
    unset(_PYTHON_VERSION_RESULT)
    unset(_VERSION)
  endif(PYTHON_EXECUTABLE)

  # If we're running v3.x look for a 2to3 utility
  if (PYTHON_VERSION_MAJOR GREATER 2)
    get_filename_component(PYTHON_EXE_DIR ${PYTHON_EXECUTABLE} PATH)
    find_program(PYTHON_2TO3_EXECUTABLE
      NAMES 2to3
      HINTS ${PYTHON_EXE_DIR}
      )

    if ("PYTHON_2TO3_EXECUTABLE-NOTFOUND" STREQUAL PYTHON_2TO3_EXECUTABLE)
      message(FATAL_ERROR "Unable to find 2to3 python utility, specify python 2.7 or specify 2to3 utility")
    endif ()
  endif (PYTHON_VERSION_MAJOR GREATER 2)


  #check for json or simplejson
  execute_process(COMMAND ${PYTHON_EXECUTABLE} -c "import json"
    RESULT_VARIABLE CHECK_PYTHON_JSON_FAILED
  )

  if (CHECK_PYTHON_JSON_FAILED)
    execute_process(COMMAND ${PYTHON_EXECUTABLE} -c "import simplejson"
      RESULT_VARIABLE CHECK_PYTHON_SIMPLEJSON_FAILED
    )

    if (CHECK_PYTHON_SIMPLEJSON_FAILED)
      message(FATAL_ERROR "REGENERATE_AMQP_FRAMING requires a python with json or simplejson modules")
    endif (CHECK_PYTHON_SIMPLEJSON_FAILED)
  endif (CHECK_PYTHON_JSON_FAILED)


  find_path(AMQP_CODEGEN_DIR
    amqp_codegen.py
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/codegen
          ${CMAKE_CURRENT_SOURCE_DIR}/rabbitmq-codegen
          ${CMAKE_CURRENT_SOURCE_DIR}/../rabbitmq-codegen
          DOC "Path to directory containing amqp_codegen.py (rabbitmq-codegen)"
          NO_DEFAULT_PATH
          )

  if (AMQP_CODEGEN_DIR STREQUAL "AMQP_CODEGEN_DIR-NOTFOUND")
    message(SEND_ERROR "REGENERATE_AMQP_FRAMING requires the amqp_codegen.py script. If this is a git clone you can:\n\ngit submodule init\ngit submodule update\n\n Or set AMQP_CODEGEN_DIR to directory containing amqp_codegen.py")
  else ()
    message(STATUS "Found amqp_codegen.py in ${AMQP_CODEGEN_DIR}")
  endif()
endif (REGENERATE_AMQP_FRAMING)

find_package(Doxygen)

option(BUILD_SHARED_LIBS "Build rabbitmq-c as a shared library" ON)
option(BUILD_STATIC_LIBS "Build rabbitmq-c as a static library" OFF)

option(BUILD_EXAMPLES "Build Examples" ON)
option(BUILD_TESTS "Build tests (run tests with make test)" ON)
option(BUILD_API_DOCS "Build Doxygen API docs" ${DOXYGEN_FOUND})

if (NOT BUILD_SHARED_LIBS AND NOT BUILD_STATIC_LIBS)
    message(FATAL_ERROR "One or both of BUILD_SHARED_LIBS or BUILD_STATIC_LIBS must be set to ON to build")
endif()

if (BUILD_TESTS)
  enable_testing()
endif (BUILD_TESTS)

if (BUILD_API_DOCS)
  if (NOT DOXYGEN_FOUND)
    message(FATAL_ERROR "Doxygen is required to build the API documentation")
  endif ()
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/docs/Doxyfile @ONLY)

  add_custom_target(docs
    COMMAND ${DOXYGEN_EXECUTABLE}
    VERBATIM
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/docs
    DEPENDS rabbitmq
    COMMENT "Generating API documentation"
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in
    )
endif ()

set(libs_private ${SOCKET_LIBRARIES} ${LIBRT})
if (ENABLE_SSL_SUPPORT)
  if (SSL_ENGINE STREQUAL "OpenSSL")
    set(requires_private "openssl")
  endif()
  if (ENABLE_THREAD_SAFETY)
    set(libs_private ${libs_private} ${CMAKE_THREAD_LIBS_INIT})
  endif()
endif()

set(prefix ${CMAKE_INSTALL_PREFIX})
set(exec_prefix "\${prefix}")
set(libdir "\${exec_prefix}/${CMAKE_INSTALL_LIBDIR}")
set(includedir "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")

configure_file(librabbitmq.pc.in ${CMAKE_CURRENT_BINARY_DIR}/librabbitmq.pc @ONLY)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/librabbitmq.pc
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
  )

if (BUILD_SHARED_LIBS)
  message(STATUS "Building rabbitmq as a shared library - yes")
else ()
  message(STATUS "Building rabbitmq as a shared library - no")
endif ()

if (BUILD_STATIC_LIBS)
  message(STATUS "Building rabbitmq as a static library - yes")
else ()
  message(STATUS "Building rabbitmq as a static library - no")
endif ()
