//
// Created by sigsegv on 04.05.2021.
//

#ifndef JEOKERNEL_CRITICAL_SECTION_H
#define JEOKERNEL_CRITICAL_SECTION_H

class critical_section {
private:
    bool activated, was_activated;
public:
    critical_section(bool enter = true);
    ~critical_section();
    void enter();
    void leave();

    critical_section(critical_section &&) = delete;
    critical_section(const critical_section &) = delete;
    critical_section &operator =(critical_section &&) = delete;
    critical_section &operator =(const critical_section &) = delete;
};

#endif //JEOKERNEL_CRITICAL_SECTION_H
