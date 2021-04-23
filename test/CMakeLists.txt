
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUNIT_TESTING ")

add_executable(mallocator mallocator.cpp)

add_test(NAME Mallocator COMMAND mallocator)
