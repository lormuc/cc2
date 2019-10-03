#pragma once

#include <vector>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <string>
#include <ostream>
#include <iostream>

#define _ auto

using str = std::string;
using std::cout;

template<class t>
using vec = std::vector<t>;

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
    t_compile_error(const str& n_str, t_loc n_loc = t_loc())
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

template <class t>
auto has(const vec<t>& c, const t& e) {
    return std::find(c.begin(), c.end(), e) != c.end();
}

str read_file_into_string(std::ifstream&);
void print_bytes(const str&, std::ostream&);
