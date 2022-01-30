//
// Created by sigsegv on 1/27/22.
//

#include "kshell.h"
#include "keyboard/keyboard.h"
#include "sstream"

kshell::kshell() : mtx(), exit(false), shell([this] () { std::this_thread::set_name("[kshell]"); run(); }) {
}

kshell::~kshell() {
    {
        std::lock_guard lock{mtx};
        exit = true;
    }
    shell.join();
}

[[noreturn]] void kshell::run() {
    std::string input{};
    while(true) {
        std::shared_ptr<keyboard_line_consumer> consumer{new keyboard_line_consumer()};
        get_klogger() << "# ";
        Keyboard().consume(consumer);
        keyboard_blocking_string *stringGetter = consumer->GetBlockingString();
        input.clear();
        {
            const std::string &keyboardString = stringGetter->GetString();
            get_klogger() << "\n";
            input.append(keyboardString);
        }
        std::vector<std::string> parsed{Parse(input)};
        if (!parsed.empty()) {
            Exec(parsed);
        }
    }
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
        get_klogger() << str.str().c_str();
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
                cmdexec->Exec(cmd);
                return;
            }
        }
        std::stringstream str{};
        str << "error: command not found: " << cmdstr << "\n";
        get_klogger() << str.str().c_str();
    }
}

void kshell::AddCommand(std::shared_ptr<kshell_command> command) {
    commands.push_back(command);
}
