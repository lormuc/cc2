#include "misc.hpp"

str read_file_into_string(std::ifstream& t) {
    t.seekg(0, std::ios::end);
    _ size = t.tellg();
    str buffer(size, ' ');
    t.seekg(0);
    t.read(buffer.data(), size);
    return buffer;
}

_ hex_digit(int x) {
    if (x < 10) {
        return str(1, '0' + x);
    } else {
        return str(1, 'a' + (x - 10));
    }
}

void print_bytes(const str& s, std::ostream& os) {
    for (_& ch : s) {
        if (isprint(ch)) {
            os << ch;
        } else {
            os << "\\";
            os << hex_digit(ch / 16) + hex_digit(ch % 16);
        }
    }
}
