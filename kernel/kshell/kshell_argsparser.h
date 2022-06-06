//
// Created by sigsegv on 6/6/22.
//

#ifndef JEOKERNEL_KSHELL_ARGSPARSER_H
#define JEOKERNEL_KSHELL_ARGSPARSER_H

#include <functional>
#include <vector>
#include <string>

class kshell_argsparser {
private:
    std::function<int (char)> params;
public:
    kshell_argsparser(const std::function<int (char)> &params);
    bool Parse(std::vector<const std::string>::iterator &iterator, const std::vector<const std::string>::iterator &iterator_end, const std::function<void (char, const std::vector<std::string> &params)> &option);
};


#endif //JEOKERNEL_KSHELL_ARGSPARSER_H
