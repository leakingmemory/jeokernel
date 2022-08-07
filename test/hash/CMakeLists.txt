
add_library(sha2alg_testobj OBJECT ../../kernel/sha2/sha2alg.cpp sha2alg_test.cpp)
target_include_directories(sha2alg_testobj PRIVATE ../../include)
