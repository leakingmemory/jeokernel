//
// Created by sigsegv on 1/27/22.
//

#include <kshell/kshell.h>
#include "keyboard/keyboard.h"
#include "sstream"
#include <tty/tty.h>
#include "kshell_stream.h"

kshell::kshell(const std::shared_ptr<class tty> &tty, std::unique_ptr<keyboard_source_interface> &&source) : source(std::move(source)), referrer("kshell"), tty(tty) {
}

void kshell::Init(const std::shared_ptr<kshell> &selfRefIn) {
    std::shared_ptr<kshell> selfRef{selfRefIn};
    std::weak_ptr<kshell> weakPtr{selfRef};
    this->selfRef = weakPtr;
    std::thread shell{[selfRef] () mutable {
        std::this_thread::set_name("[kshell]");
        selfRef->run();
    }};
    this->shell = std::move(shell);
}

std::shared_ptr<kshell> kshell::Create(const std::shared_ptr<class tty> &tty, std::unique_ptr<keyboard_source_interface> &&source) {
    std::shared_ptr<kshell> shell{new kshell(tty, std::move(source))};
    shell->Init(shell);
    return shell;
}

std::string kshell::GetReferrerIdentifier() {
    return "";
}

kshell::~kshell() {
    {
        std::lock_guard lock{mtx};
        exit = true;
    }
    shell.join();
}

void kshell::OnExit(const std::function<void()> &func) {
    std::lock_guard lock{mtx};
    exitFuncs.push_back(func);
}

void kshell::CwdRoot() {
    std::shared_ptr<class referrer> selfRef = this->selfRef.lock();
    this->cwd_ref = get_kernel_rootdir()->CreateReference(selfRef);
    this->cwd = dynamic_cast<kdirectory *> (&(*(this->cwd_ref)));
}

void kshell::Cwd(const reference<kfile> &cwd) {
    std::shared_ptr<class referrer> selfRef = this->selfRef.lock();
    this->cwd_ref = cwd.CreateReference(selfRef);
    this->cwd = dynamic_cast<kdirectory *> (&(*(this->cwd_ref)));
}

reference<kfile> kshell::CwdRef(std::shared_ptr<class referrer> &referrer) {
    return cwd_ref.CreateReference(referrer);
}

void kshell::run() {
    std::string input{};
    CwdRoot();
    while (!exit) {
        std::shared_ptr<keyboard_line_consumer> consumer{new keyboard_line_consumer(tty->GetOutput())};
        tty->Write("# ", 2);
        source->consume(consumer);
        keyboard_blocking_string *stringGetter = consumer->GetBlockingString();
        input.clear();
        {
            const std::string &keyboardString = stringGetter->GetString();
            tty->Write("\n", 1);
            input.append(keyboardString);
        }
        std::vector<std::string> parsed{Parse(input)};
        if (!parsed.empty()) {
            Exec(parsed);
        }
    }
    std::vector<std::function<void ()>> exitFuncs{};
    {
        std::lock_guard lock{mtx};
        for (const auto &func : this->exitFuncs) {
            exitFuncs.push_back(func);
        }
    }
    for (auto &func : exitFuncs) {
        func();
    }
}

void kshell::Print(const char *str) const {
    auto len = strlen(str);
    tty->Write(str, len);
}

std::vector<std::string> kshell::Parse(const std::string &cmd) {
    std::vector<std::string> parsed{};

    std::string tmp{};
    bool white{true};
    bool quoted{false};
    char quote = '\0';
    auto iterator = cmd.cbegin();
    while (iterator != cmd.cend()) {
        if (white && is_white(*iterator)) {
            ++iterator;
            continue;
        }
        if (white) {
            white = false;
            quote = '\0';
        }
        if (!quoted && is_white(*iterator)) {
            parsed.push_back((const typeof(tmp)) tmp);
            white = true;
            tmp.clear();
            ++iterator;
            continue;
        }
        if (quoted && *iterator == quote) {
            ++iterator;
            quoted = false;
            continue;
        }
        if (!quoted && *iterator == '"' || *iterator == '\'') {
            quote = *iterator;
            ++iterator;
            quoted = true;
            continue;
        }
        if (quoted && *iterator == '\\') {
            ++iterator;
            if (iterator != cmd.cend()) {
                auto escaped = *iterator;
                ++iterator;
                switch (escaped) {
                    case 'n':
                        tmp.append("\n");
                        break;
                    case 'r':
                        tmp.append("\r");
                        break;
                    case 't':
                        tmp.append("\t");
                        break;
                    default:
                        tmp.append(&escaped, 1);
                }
            }
            continue;
        }
        auto ch = *iterator;
        ++iterator;
        tmp.append(&ch, 1);
    }
    if (quoted) {
        std::stringstream str{};
        str << "error: unterminated " << quote << "\n";
        Print(str.str().c_str());
        return {};
    }
    if (tmp.size() > 0 || quote != '\0') {
        parsed.push_back(tmp);
    }
    return parsed;
}

void kshell::Exec(const std::vector<std::string> &cmd) {
    if (!cmd.empty()) {
        std::string cmdstr = cmd[0];
        for (auto cmdexec : commands) {
            if (cmdexec->Command() == cmdstr) {
                cmdexec->Exec(*this, cmd);
                return;
            }
        }
        std::stringstream str{};
        str << "error: command not found: " << cmdstr << "\n";
        Print(str.str().c_str());
    }
}

void kshell::AddCommand(std::shared_ptr<kshell_command> command) {
    commands.push_back(command);
}

std::unique_ptr<keyboard_source_interface> kshell::In() const {
    return source->clone();
}

kshell_stream kshell::Out() {
    return {selfRef.lock()};
}

kshell_stream kshell::Err() {
    return {selfRef.lock()};
}
