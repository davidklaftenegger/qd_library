add_subdirectory(gtest-1.7.0)
include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

set(genfiles
	generated_test0.cpp
	generated_test1.cpp
	generated_test2.cpp
	generated_test3.cpp
	generated_test4.cpp
	generated_test5.cpp
	generated_test6.cpp
	generated_test7.cpp)

add_custom_command(
	OUTPUT ${genfiles}
	COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/gentests.py
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	DEPENDS gentests.py)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_executable(lockTests lock.hpp ${genfiles})
target_link_libraries(lockTests qd gtest gtest_main)

