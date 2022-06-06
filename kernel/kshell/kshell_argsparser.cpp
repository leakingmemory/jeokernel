//
// Created by sigsegv on 6/6/22.
//

#include <iostream>
#include "kshell_argsparser.h"

kshell_argsparser::kshell_argsparser(const std::function<int(char)> &params) : params(params){
}

struct kshell_argparser_consume {
    std::vector<std::string> params;
    int remaining;
    char opt;
};

bool kshell_argsparser::Parse(std::vector<const std::string>::iterator &iterator, const std::vector<const std::string>::iterator &iterator_end,
                              const std::function<void(char, const std::vector<std::string> &)> &f_option) {
    std::function<void (char, const std::vector<std::string> &params)> option{f_option};
    std::vector<kshell_argparser_consume> opts;
    while (iterator != iterator_end) {
        std::string par = *iterator;
        if (par.starts_with("-")) {
            {
                std::string parwithout{};
                parwithout.append(par.c_str() + 1);
                par = parwithout;
            }
            if (!par.starts_with("-")) {
                for (char ch : par) {
                    int num = this->params(ch);
                    if (num == 0) {
                        option(ch, {});
                    } else if (num > 0) {
                        opts.push_back({.params = {}, .remaining = num, .opt = ch});
                    } else {
                        std::cerr << "Error at param " << *iterator << "\n";
                        return false;
                    }
                }
                ++iterator;
                continue;
            } else {
                par = *iterator;
            }
        }
        if (opts.empty()) {
            return true;
        }
        auto optiter = opts.begin();
        (*optiter).params.push_back(par);
        (*optiter).remaining--;
        if ((*optiter).remaining == 0) {
            option((*optiter).opt, (*optiter).params);
            opts.erase(optiter);
            if (opts.empty()) {
                ++iterator;
                return true;
            }
        }
        ++iterator;
    }
    if (!opts.empty()) {
        std::cerr << "Expected additional parameters\n";
        return false;
    }
    return true;
}