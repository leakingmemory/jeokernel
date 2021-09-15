
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUNIT_TESTING ")

add_executable(mallocator mallocator.cpp)
add_executable(xhdi_disable_legsup xhdi_disable_legsup.cpp)
target_link_libraries(xhdi_disable_legsup PRIVATE -lpthread)
add_executable(sharedptr sharedptr.cpp)

add_test(NAME Mallocator COMMAND mallocator)
add_test(NAME Xhdi_disable_legsup COMMAND xhdi_disable_legsup)
add_test(NAME Sharedptr COMMAND sharedptr)
