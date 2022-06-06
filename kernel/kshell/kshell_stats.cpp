//
// Created by sigsegv on 3/9/22.
//

#include "kshell_stats.h"
#include <stats/statistics_root.h>
#include <sstream>
#include <klogger.h>

class kshell_stats_visitor : public statistics_visitor {
private:
    std::string indent;
public:
    kshell_stats_visitor(const std::string &indent) : indent(indent) {}
    void Visit(const std::string &name, statistics_object &object) override;
    virtual void Visit(const std::string &name, long long int value) override;
};

void kshell_stats_visitor::Visit(const std::string &name, statistics_object &object) {
    {
        std::stringstream str{};
        str << indent << name << ":\n";
        get_klogger() << str.str().c_str();
    }
    std::string subindent{indent};
    subindent.append("  ");
    kshell_stats_visitor sub{subindent};
    object.Accept(sub);
}

void kshell_stats_visitor::Visit(const std::string &name, long long value) {
    std::stringstream str{};
    str << indent << name << ": " << value << "\n";
    get_klogger() << str.str().c_str();
}

void kshell_stats::Exec(kshell &, const std::vector<std::string> &cmd) {
    kshell_stats_visitor visitor{""};
    GetStatisticsRoot().Accept(visitor);
}
