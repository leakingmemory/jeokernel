//
// Created by sigsegv on 04.05.2021.
//

#ifndef JEOKERNEL_CRITICAL_SECTION_H
#define JEOKERNEL_CRITICAL_SECTION_H

class critical_section_host;

class critical_section {
    friend critical_section_host;
private:
    bool entered;
    bool activated;
    bool was_activated;
public:
    static bool has_interrupts_disabled();

    critical_section(bool enter = true);
    ~critical_section();
    void enter();
    void leave();

    critical_section(critical_section &&) = delete;
    critical_section(const critical_section &) = delete;
    critical_section &operator =(critical_section &&) = delete;
    critical_section &operator =(const critical_section &) = delete;
};

class critical_section_host : public critical_section {
public:
    critical_section_host() : critical_section(false) {}

    critical_section_host(critical_section_host &&) = delete;
    critical_section_host(const critical_section_host &) = delete;

    critical_section_host &operator =(critical_section_host &&) = delete;
    critical_section_host &operator =(const critical_section_host &) = delete;

    critical_section_host &operator =(critical_section &&mv) {
        this->entered = mv.entered;
        this->activated = mv.activated;
        this->was_activated = mv.was_activated;
        mv.entered = false;
        return *this;
    }

    void extract(critical_section &cli) {
        cli.entered = entered;
        cli.activated = activated;
        cli.was_activated = was_activated;
        entered = false;
    }
};

#endif //JEOKERNEL_CRITICAL_SECTION_H
