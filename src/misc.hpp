#pragma once

#include <vector>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <string>

template<class t>
auto has(const std::vector<t>& c, const t& e) {
    return std::find(c.begin(), c.end(), e) != c.end();
}

struct t_loc {
    unsigned line;
    unsigned column;
};

class t_error : public std::runtime_error {
    t_loc loc;
public:
    t_error(const char* n_str, const t_loc& n_loc)
        : std::runtime_error(n_str) {
        loc = n_loc;
    }
    t_error(const std::string& n_str, const t_loc& n_loc)
        : std::runtime_error(n_str) {
        loc = n_loc;
    }
    unsigned line() const {
        return loc.line;
    }
    unsigned column() const {
        return loc.column;
    }
};

typedef unsigned long long ull;

std::string read_file_into_string(std::ifstream&);
std::string print_bytes(const std::string&);
void err(const std::string&, t_loc);
