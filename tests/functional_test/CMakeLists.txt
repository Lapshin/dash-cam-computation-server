cmake_minimum_required(VERSION 3.9)
project(functional_test_lib C)

find_program (BASH_PROGRAM bash)

if (BASH_PROGRAM)
  add_test (mytest ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/test.sh)
endif (BASH_PROGRAM)

AUX_SOURCE_DIRECTORY(lib/ SRC_LIST)

ADD_LIBRARY(${PROJECT_NAME} SHARED ${SRC_LIST})

#set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "-m32" LINK_FLAGS "-m32")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/api)
