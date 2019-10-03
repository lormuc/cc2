#pragma once

#include <string>
#include <list>

#include "misc.hpp"

struct t_lexeme {
    str uu;
    str vv;
    t_loc loc;
};

std::list<t_lexeme> lex(const str&);
