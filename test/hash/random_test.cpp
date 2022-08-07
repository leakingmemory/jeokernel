//
// Created by sigsegv on 8/7/22.
//

#include "../../kernel/random/prng.h"

extern "C" {
    void random_print(unsigned int num1, unsigned int num2, unsigned int num3);

    int random_test() {
        std::shared_ptr<prng> random = CreatePrng();
        unsigned int num1;
        unsigned int num2;
        unsigned int num3;
        random->Seed(nullptr, 0);
        random->Generate(&num1, sizeof(num1));
        random->Seed(nullptr, 0);
        random->Generate(&num2, sizeof(num2));
        random->Seed(nullptr, 0);
        random->Generate(&num3, sizeof(num3));
        random_print(num1, num2, num3);
        return 0;
    }
}