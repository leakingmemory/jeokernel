
add_subdirectory(hash)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUNIT_TESTING ")

add_executable(mallocator mallocator.cpp)
#add_executable(xhdi_disable_legsup xhdi_disable_legsup.cpp)
#target_link_libraries(xhdi_disable_legsup PRIVATE -lpthread)
add_executable(sharedptr sharedptr.cpp)
add_executable(stdvector stdvector.cpp)
add_executable(memmove memmove.cpp)
add_executable(sha2alg_test sha2alg_test.cpp)
target_link_libraries(sha2alg_test PRIVATE sha2alg_testobj)

add_test(NAME Mallocator COMMAND mallocator)
#add_test(NAME Xhdi_disable_legsup COMMAND xhdi_disable_legsup)
add_test(NAME Sharedptr COMMAND sharedptr)
add_test(NAME StdVector COMMAND stdvector)
add_test(NAME MemMove COMMAND memmove)
add_test(NAME Sha2Alg COMMAND sha2alg_test)
