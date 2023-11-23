//
// Created by sigsegv on 12/26/21.
//

#include "tests.h"
#include <cstdint>
#include "../kernel/strings.cpp"

int main() {
    uint64_t comp[] = {0x1234567812345678, 0, 0x1234567812345678, 0, 0x1234567812345678, 0x1234567812345678};
    uint64_t test1[] = {0x1234567812345678, 0, 0x1234567812345678, 0, 0x1234567812345678, 0x1234567812345678};
    uint64_t test2[] = {0, 0, 0, 0, 0, 0};
    memmove(test2, test1, 5 * 8);
    assert(test2[0] == comp[0]);
    assert(test2[1] == comp[1]);
    assert(test2[2] == comp[2]);
    assert(test2[3] == comp[3]);
    assert(test2[4] == comp[4]);
    memmove(test2, ((uint8_t *) (void *) test1) + 1, 5 * 8);
    assert(test2[0] == 0x0012345678123456);
    assert(test2[1] == 0x7800000000000000);
    assert(test2[2] == 0x0012345678123456);
    assert(test2[3] == 0x7800000000000000);
    assert(test2[4] == 0x7812345678123456);
    assert(test2[5] == 0);
    return 0;
}