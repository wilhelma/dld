include (GenerateExportHeader)

project(DLD)
cmake_minimum_required(VERSION 2.8)

aux_source_directory(. SRC_LIST)

set(CMAKE_BUILD_DIR ${CMAKE_SOURCE_DIR}/build})

# --------------------------------- Pin related search map ---------------------------------------- #

find_path(PIN_ROOT_DIR
  NAMES source/include/pin/pin.H
  PATHS $ENV{PIN_ROOT_DIR}
  DOC "Pin's base directory"
)
if(NOT PIN_ROOT_DIR)
  message(FATAL_ERROR
    "\nPin not found!\n"
    "Please set the environment variable PIN_ROOT_DIR to the base directory"
    " of the Pin.\n Please make sure the same pin installation is in USER or SYSTEM PATH as well"
  )
endif(NOT PIN_ROOT_DIR)
message(STATUS "PIN_ROOT_DIR: ${PIN_ROOT_DIR}")

set(PIN_INCLUDE_DIRS
  ${PIN_ROOT_DIR}/extras/xed2-intel64/include
  ${PIN_ROOT_DIR}/source/include/pin
  ${PIN_ROOT_DIR}/source/include/pin/gen
  ${PIN_ROOT_DIR}/extras/components/include
  ${PIN_ROOT_DIR}/source/tools/InstLib
)
set(PIN_LIBRARY_DIRS
  ${PIN_ROOT_DIR}/extras/components/lib/intel64
  ${PIN_ROOT_DIR}/extras/xed2-intel64/lib
  ${PIN_ROOT_DIR}/intel64/lib
  ${PIN_ROOT_DIR}/intel64/lib-ext
)

# ---------------------------------------------------------------------------------------------------- #

include_directories(${BOOST_INC_PATH} ${PIN_INCLUDE_DIRS} ${SQLITE_INC_PATH})
link_directories ( ${BOOST_LIB_PATH} ${PIN_LIBRARY_DIRS} ${SQLITE_LIB_PATH})

SET(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_FLAGS "-DBIGARRAY_MULTIPLIER=1 -Wall -Wno-unknown-pragmas -std=c++11 -Wl,-Bsymbolic -fPIC -O0 -g")
set(CMAKE_C_FLAGS "-DBIGARRAY_MULTIPLIER=1 -WALL -Wno-unknown-pragmas -std=c++11 -Wl,-Bsymbolic -fPIC -O0 -g")
set(CMAKE_SHARED_LINKER_FLAGS "-Wl,-Bsymbolic ${CMAKE_SHARED_LINKER_FLAGS} ")
add_definitions(-DBIGARRAY_MULTIPLIER=1 -DTARGET_IA32E -DHOST_IA32E -DTARGET_LINUX)

set(CMAKE_SHARED_LINKER_FLAGS " ${PARCEIVE_LINKER_FLAGS} ${CMAKE_SHARED_LINKER_FLAGS} ")

add_library(${PROJECT_NAME} SHARED ${SRC_LIST})
