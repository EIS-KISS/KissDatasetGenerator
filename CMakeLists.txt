cmake_minimum_required(VERSION 3.0)

project(kissdatasetgenerator C CXX)
set (CMAKE_CXX_STANDARD 20)

set(SRC_FILES
	src/main.cpp
	src/log.cpp
	src/tokenize.cpp
	src/randomgen.cpp
	src/hash.cpp
	src/datasets/eisgendata.cpp
	src/datasets/eisdataset.cpp
	src/datasets/eisgendatanoise.cpp
	src/datasets/parameterregressiondataset.cpp
	src/microtar.c)

find_package(PkgConfig REQUIRED)
pkg_search_module(DRT REQUIRED libeisdrt)
pkg_search_module(EIS REQUIRED libeisgenerator)
pkg_search_module(TYPE REQUIRED libkisstype)
pkg_search_module(NOISE REQUIRED libeisnoise)

add_executable(${PROJECT_NAME} ${SRC_FILES})
target_link_libraries( ${PROJECT_NAME} ${DRT_LIBRARIES} -lpthread ${EIS_LIBRARIES} ${NOISE_LIBRARIES} ${TYPE_LIBRARIES})
target_include_directories(${PROJECT_NAME} PRIVATE ${EIS_INCLUDE_DIRS} ${DRT_INCLUDE_DIRS} ${NOISE_INCLUDE_DIRS} ${TYPE_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/src)
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
