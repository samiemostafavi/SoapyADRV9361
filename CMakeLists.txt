########################################################################
# Project setup
########################################################################
cmake_minimum_required(VERSION 2.8.9)
project(SoapyAdrvSDR CXX)

# select build type to get optimization flags
if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE "Release")
   message(STATUS "Build type not specified: defaulting to release.")
endif(NOT CMAKE_BUILD_TYPE)
set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "")

find_package(SoapySDR NO_MODULE)
 if (NOT SoapySDR_FOUND) 
    message(FATAL_ERROR "Soapy SDR development files not found...") 
 endif () 


#enable c++11 features
if(CMAKE_COMPILER_IS_GNUCXX)

    #C++11 is a required language feature for this project
    include(CheckCXXCompilerFlag)
    CHECK_CXX_COMPILER_FLAG("-std=c++11" HAS_STD_CXX11)
    if(HAS_STD_CXX11)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    else(HAS_STD_CXX11)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
    endif()

    #Thread support enabled (not the same as -lpthread)
    list(APPEND LIBIIO_LIBRARIES -pthread)

    #disable warnings for unused parameters
    add_definitions(-Wno-unused-parameter)
endif(CMAKE_COMPILER_IS_GNUCXX)

#enable c++11 extensions for OSX
if (APPLE)
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -Wc++11-extensions")
endif(APPLE)

SOAPY_SDR_MODULE_UTIL(
    TARGET AdrvSDRSupport
    SOURCES
    AdrvSDR_Registration.cpp
    AdrvSDR_Settings.cpp
    AdrvSDR_Streaming.cpp
    UDPClient.cpp
)

aux_source_directory(${PROJECT_SOURCE_DIR} SRC_LIST)

# net test library
set(SHARED_SRC ${PROJECT_SOURCE_DIR}/UDPClient.cpp)
list(REMOVE_ITEM SRC_LIST ${SHARED_SRC})
add_library(NET_TEST_OBJECTS OBJECT ${SHARED_SRC})
set(SRC_LIST ${SRC_LIST} $<TARGET_OBJECTS:NET_TEST_OBJECTS>)

ADD_SUBDIRECTORY(tests)



