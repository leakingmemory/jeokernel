
add_subdirectory(hash)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUNIT_TESTING ")

add_executable(mallocator mallocator.cpp testassert.cpp)
#add_executable(xhdi_disable_legsup xhdi_disable_legsup.cpp testassert.cpp)
#target_link_libraries(xhdi_disable_legsup PRIVATE -lpthread)
add_executable(sharedptr sharedptr.cpp testassert.cpp)
add_executable(weakptr weakptr.cpp testassert.cpp)
add_executable(stdvector stdvector.cpp testassert.cpp)
add_executable(stdvariant stdvariant.cpp testassert.cpp)
add_executable(memmove memmove.cpp testassert.cpp)
add_executable(sha2alg_test sha2alg_test.cpp hash_tests.cpp testassert.cpp)
target_link_libraries(sha2alg_test PRIVATE hash_testobj)
add_executable(random_test random_test.cpp hash_tests.cpp testassert.cpp)
target_link_libraries(random_test PRIVATE hash_testobj)
add_executable(stdstring stdstring.cpp testassert.cpp)
# The loader-mode physpagemap test pulls in physpagemap.cpp with -DLOADER, which
# uses angle-bracket includes (<physpagemap.h>, <std/limits.h>) needing the
# project include dir. That dir must NOT leak into testassert.cpp, or its system
# <iostream> would resolve to the freestanding one, so scope it to this source.
add_executable(physpagemap_loader physpagemap_loader.cpp testassert.cpp)
target_compile_definitions(physpagemap_loader PRIVATE LOADER)
set_source_files_properties(physpagemap_loader.cpp PROPERTIES
        INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR}/include)

add_test(NAME Mallocator COMMAND mallocator)
#add_test(NAME Xhdi_disable_legsup COMMAND xhdi_disable_legsup)
add_test(NAME Sharedptr COMMAND sharedptr)
add_test(NAME Weakptr COMMAND weakptr)
add_test(NAME StdVector COMMAND stdvector)
add_test(NAME StdVariant COMMAND stdvariant)
add_test(NAME MemMove COMMAND memmove)
add_test(NAME Sha2Alg COMMAND sha2alg_test)
add_test(NAME Random COMMAND random_test)
add_test(NAME StdString COMMAND stdstring)
add_test(NAME PhysPageMapLoader COMMAND physpagemap_loader)
