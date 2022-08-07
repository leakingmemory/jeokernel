//
// Created by sigsegv on 8/6/22.
//

#include <sstream>
#include "../../include/hash/sha2alg.h"

extern "C" {
    void sha2alg_print(const char *str);

    int sha2alg_test() {
        {
            sha256 h{};
            h.Final(nullptr, 0);
            std::string str{h.Hex()};
            std::stringstream output{};
            output << "sha256(\"\") = " << str << "\n";
            sha2alg_print(output.str().c_str());
            if (str != "E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855") {
                return -1;
            }
        }
        {
            std::string input{"hello"};
            sha256 h{};
            h.Final((uint8_t *) input.c_str(), input.size());
            std::string str{h.Hex()};
            std::stringstream output{};
            output << "sha256(\""<< input <<"\") = " << str << "\n";
            sha2alg_print(output.str().c_str());
            if (str != "2CF24DBA5FB0A30E26E83B2AC5B9E29E1B161E5C1FA7425E73043362938B9824") {
                return -1;
            }
        }
        {
            std::string input{"1234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ{}"};
            sha256 h{};
            h.Consume((uint8_t *) input.c_str());
            h.Final(nullptr, 0);
            std::string str{h.Hex()};
            std::stringstream output{};
            output << "sha256(\""<< input <<"\") = " << str << "\n";
            sha2alg_print(output.str().c_str());
            if (str != "49A3FC0038B5CD9EC1134C6FCB1C98518FBA5C370EFEBD4B759A90B2FDE50E16") {
                return -1;
            }
        }
        sha2alg_print("SHA2 passed");
        return 0;
    }
}
