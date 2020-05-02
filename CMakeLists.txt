cmake_minimum_required(VERSION 3.9)

if ( ${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR} )
    MESSAGE( FATAL_ERROR "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there. You may need to remove CMakeCache.txt." )
endif()

ENABLE_TESTING()
SET(CMAKE_C_STANDARD 99)

SET(CMAKE_C_FLAGS "-pedantic -Wall -Wextra -Werror -O2 -fPIC -g -MD")
#SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wshadow -Wlogical-op -Wshift-overflow=2 -Wduplicated-cond -Wconversion -fstack-check -fstack-protector-all -ftrapv -D_FORTIFY_SOURCE=2 ")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fsanitize=undefined -fno-sanitize-recover ")

ADD_SUBDIRECTORY(lib)
ADD_SUBDIRECTORY(tests)

PROJECT(computation-server C)

SET(THREADS_PREFER_PTHREAD_FLAG ON)
FIND_PACKAGE(Threads REQUIRED)
AUX_SOURCE_DIRECTORY(src/ SRC_LIST)

ADD_EXECUTABLE(${PROJECT_NAME} ${SRC_LIST})
#set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "-m32" LINK_FLAGS "-m32")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/api ${CMAKE_SOURCE_DIR}/include)
TARGET_LINK_LIBRARIES(${PROJECT_NAME} indicator pthread rt)

ADD_DEPENDENCIES(${PROJECT_NAME} indicator)
