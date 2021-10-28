//
// Created by sigsegv on 7/5/21.
//

#include <ctype.h>

int isalnum(int c) {
    return isalpha(c) || isdigit(c);
}

int isalpha(int c) {
    return (c <= 'Z' && c >= 'A') || (c <= 'z' && c >= 'a') || (c <= '9' && c >= '0');
}
int isdigit(int c) {
    return (c <= '9' && c >= '0');
}
int isxdigit(int c) {
    return (c <= '9' && c >= '0') || (c <= 'F' && c >= 'A') || (c <= 'f' && c >= 'a');
}
int isprint(int c) {
    return c >= 0x20 && c < 0x80;
}
int islower(int c) {
    return (c <= 'z' && c >= 'a');
}
int isupper(int c) {
    return (c <= 'Z' && c >= 'A');
}
int isspace(int c) {
    return c == ' ' || c == '\t' || c == '\v' || c == '\f' || c == '\n' || c == '\r';
}

int toupper(int c) {
    if (islower(c)) {
        return c + 'A' - 'a';
    } else {
        return c;
    }
}

int tolower(int c) {
    if (isupper(c)) {
        return c + 'a' - 'A';
    } else {
        return c;
    }
}
