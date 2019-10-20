#pragma once

#include <list>
#include <set>
#include <ostream>

#include "misc.hpp"
#include "file.hpp"

struct t_pp_lexeme {
    str kind;
    str val;
    t_loc loc = t_loc();
    std::set<str> hide_set = {};
};

std::list<t_pp_lexeme> lex(size_t, const t_file_manager& fm);
void print(const std::list<t_pp_lexeme>& ls, std::ostream& os,
           const str& separator = "");
str pp_kind(const str&);
