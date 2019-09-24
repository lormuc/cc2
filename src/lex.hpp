#pragma once

#include <string>
#include <initializer_list>
#include <array>
#include <list>

#include "misc.hpp"

struct t_lexeme {
    std::string uu;
    std::string vv;
    t_loc loc;
};

std::list<t_lexeme> lex(const std::string&);
