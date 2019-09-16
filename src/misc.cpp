#include "misc.hpp"

using namespace std;

string read_file_into_string(ifstream& t) {
    t.seekg(0, ios::end);
    size_t size = t.tellg();
    string buffer(size, ' ');
    t.seekg(0);
    t.read(buffer.data(), size);
    return buffer;
}

auto hex_digit(int x) {
    if (x < 10) {
        return string(1, '0' + x);
    } else {
        return string(1, 'a' + (x - 10));
    }
}

string print_bytes(const string& str) {
    string res;
    for (auto& ch : str) {
        if (isprint(ch)) {
            res += ch;
        } else {
            res += "\\";
            res += hex_digit(ch / 16) + hex_digit(ch % 16);
        }
    }
    return res;
}
