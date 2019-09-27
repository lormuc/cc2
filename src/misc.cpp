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

str print_bytes(const str& s) {
    str res;
    for (_& ch : s) {
        if (isprint(ch)) {
            res += ch;
        } else {
            res += "\\";
            res += hex_digit(ch / 16) + hex_digit(ch % 16);
        }
    }
    return res;
}
