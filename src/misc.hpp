#pragma once

#include <vector>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <string>

struct t_loc {
    int line = -1;
    int column = -1;
    bool operator==(const t_loc& x) const {
        return line == x.line and column == x.column;
    }
    bool operator!=(const t_loc& x) const {
        return !(*this == x);
    }
    bool operator<(const t_loc& x) const {
        return line < x.line or (line == x.line and column < x.column);
    }
    bool operator>(const t_loc& x) const {
        return x < *this;
    }
    t_loc(int x, int y)
        : line(x)
        , column(y) {
    }
    t_loc()
        : line(-1)
        , column(-1) {
    }
};

class t_compile_error : public std::runtime_error {
    t_loc loc;
public:
    t_compile_error(const std::string& n_str, t_loc n_loc = t_loc())
        : std::runtime_error(n_str), loc(n_loc) {
    }
    int line() const {
        return loc.line;
    }
    int column() const {
        return loc.column;
    }
    t_loc get_loc() const {
        return loc;
    }
};

typedef unsigned long long ull;
typedef unsigned ui;

template <class t>
auto has(const std::vector<t>& c, const t& e) {
    return std::find(c.begin(), c.end(), e) != c.end();
}

std::string read_file_into_string(std::ifstream&);
std::string print_bytes(const std::string&);
