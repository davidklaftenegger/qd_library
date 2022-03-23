cmake_minimum_required(VERSION 2.8)
project(QDLibrary)
# The version number.
set(QD_VERSION_MAJOR 0)
set(QD_VERSION_MINOR 1)

set(DEFAULT_C_FLAGS "-std=c11 -pthread")
set(DEFAULT_CXX_FLAGS "-std=c++11 -pthread")
set(DEFAULT_LINK_FLAGS "")

option(QD_DEBUG
	"Build the QD library without optimization and with debugging symbols" OFF)
if(QD_DEBUG)
	set(DEFAULT_C_FLAGS "${DEFAULT_C_FLAGS} -O0 -g -Wall -Wextra -Werror")
	set(DEFAULT_CXX_FLAGS
		"${DEFAULT_CXX_FLAGS} -O0 -g -Wall -Wextra -Werror")
	set(DEFAULT_LINK_FLAGS "${DEFAULT_LINK_FLAGS} -g -Wall -Wextra -Werror")
else(QD_DEBUG)
	set(DEFAULT_C_FLAGS "${DEFAULT_C_FLAGS} -O3")
	set(DEFAULT_CXX_FLAGS "${DEFAULT_CXX_FLAGS} -O3")
	set(DEFAULT_LINK_FLAGS "${DEFAULT_LINK_FLAGS} -O3")
endif(QD_DEBUG)

set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} ${DEFAULT_C_FLAGS}")
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${DEFAULT_CXX_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} ${DEFAULT_LINK_FLAGS}")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include_directories("${PROJECT_SOURCE_DIR}")

# put together all source files 
set(qd_sources "threadid.cpp")
set(lock_sources locks/mcs_lock.cpp locks/mcs_futex_lock.cpp)

add_library(qd SHARED ${qd_sources} ${lock_sources})

option(QD_USE_LIBNUMA
	"Use libnuma to determine NUMA structure" ON)
if(QD_USE_LIBNUMA)
	target_link_libraries(qd numa)
	add_definitions(-DQD_USE_LIBNUMA)
endif(QD_USE_LIBNUMA)

install(TARGETS qd
	COMPONENT "Runtime"
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib)

install(DIRECTORY .
	DESTINATION include/qd/
	COMPONENT "Development"
	FILES_MATCHING
		PATTERN "*.h"
		PATTERN "*.hpp")

option(QD_TESTS
	"Build tests for QD locking" ON)
if(QD_TESTS)
	enable_testing()
	include_directories("${PROJECT_SOURCE_DIR}/tests")
	add_subdirectory(tests)
endif(QD_TESTS)
