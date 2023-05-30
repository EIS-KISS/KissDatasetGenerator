cmake_minimum_required(VERSION 3.0)

project(eisgendirdataset)
set (CMAKE_CXX_STANDARD 20)

set(SRC_FILES src/main.cpp src/log.cpp src/tokenize.cpp src/randomgen.cpp src/hash.cpp src/eisgendata.cpp src/eisdataset.cpp src/parameterregressiondataset.cpp)

find_package(PkgConfig REQUIRED)
pkg_search_module(DRT REQUIRED libeisdrt)

add_executable(${PROJECT_NAME} ${SRC_FILES})
target_link_libraries( ${PROJECT_NAME} ${DRT_LIBRARIES} -lpthread -leisgenerator)
target_include_directories(${PROJECT_NAME} ${DRT_INCLUDE_DIRS} PRIVATE)
target_compile_options(${PROJECT_NAME} PRIVATE
	"-Wall"
	"-Wno-reorder"
	"-Wfatal-errors"
	"-ffast-math"
	"-ftree-vectorize"
	"-g"
	"-fno-strict-aliasing"
	)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s")

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
