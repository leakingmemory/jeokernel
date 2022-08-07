
add_library(hash_testobj OBJECT ../../kernel/sha2/sha2alg.cpp sha2alg_test.cpp ../../kernel/random/prng.cpp ../../kernel/random/prng.h random_test.cpp)
target_include_directories(hash_testobj PRIVATE ../../include)
