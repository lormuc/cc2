#pragma once

#include <istream>

#include "misc.hpp"

struct t_file_data {
    str abs_path;
    str path;
    str file_contents;
};

class t_file_manager {
    vec<t_file_data> files;
public:
    size_t read_file(const str&, const str&);
    size_t read_file(const str&);
    const str& get_file_contents(size_t) const;
    const str& get_path(size_t) const;
    const str& get_abs_path(size_t) const;
    void clear();
};

str get_file_dir(const str&);
str get_abs_path(const str&);
str replace_extension(const str&, const str&);
