#include "misc.hpp"

_ hex_digit(int x) {
    if (x < 10) {
        return str(1, '0' + x);
    } else {
        return str(1, 'a' + (x - 10));
    }
}

void print_bytes(const str& s, std::ostream& os) {
    for (_& ch : s) {
        if (isprint(ch) and ch != '"' and ch != '\\') {
            os << ch;
        } else {
            os << "\\";
            os << hex_digit(ch / 16) + hex_digit(ch % 16);
        }
    }
}

void constrain(bool constraint, const str& msg, const t_loc& loc) {
    if (not constraint) {
        throw t_compile_error(msg, loc);
    }
}
