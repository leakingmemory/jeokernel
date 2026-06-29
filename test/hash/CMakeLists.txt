
add_library(hash_testobj OBJECT ../../kernel/sha2/sha2alg.cpp sha2alg_test.cpp ../../kernel/random/prng.cpp ../../kernel/random/prng.h random_test.cpp)
target_include_directories(hash_testobj PRIVATE ../../include)
# These sources include the project's freestanding <sstream>/<string>/... (via
# the include dir above), which define weak std:: symbols (ios_base, locale,
# basic_string, ...). With default visibility they leak into the executable's
# dynamic symbol table and interpose the host libstdc++, corrupting its pre-main
# static init and crashing before main(). Hide them so they stay local.
target_compile_options(hash_testobj PRIVATE -fvisibility=hidden -fvisibility-inlines-hidden)
