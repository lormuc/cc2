#pragma once

#include <string>
#include <vector>
#include <list>

#include "lex.hpp"
#include "misc.hpp"

class t_parse_error : public t_compile_error {
public:
    t_parse_error(const str& str, t_loc loc)
        : t_compile_error("parse error: " + str, loc) {
    }
    t_parse_error(t_loc loc)
        : t_compile_error("", loc) {
    }
};

struct t_ast {
    str uu;
    str vv;
    vec<t_ast> children;
    t_loc loc;

    t_ast() {}

    void add_child(const t_ast& x) {
        children.push_back(x);
    }

    t_ast(const str& u, const str& v,
          const vec<t_ast>& c) {
        uu = u;
        vv = v;
        children = c;
    }

    t_ast(const str& u) {
        uu = u;
    }

    t_ast(const str& u, const vec<t_ast>& c) {
        uu = u;
        children = c;
    }

    t_ast(const str& u, const str& v) {
        uu = u;
        vv = v;
    }

    t_ast(const vec<t_ast>& c) {
        children = c;
    }

    t_ast(const str& u, t_loc _loc) {
        uu = u;
        loc = _loc;
    }

    t_ast(const str& u, const str& v, t_loc _loc) {
        uu = u;
        vv = v;
        loc = _loc;
    }

    bool operator==(const t_ast& a) const {
        return uu == a.uu and vv == a.vv and children == a.children;
    }

    bool operator!=(const t_ast& x) const {
        return not (*this == x);
    }

    t_ast& operator[](std::size_t i) {
        return children[i];
    }

    const t_ast& operator[](std::size_t i) const {
        return children[i];
    }
};

t_ast parse_program(const std::list<t_lexeme>&);

extern const vec<str> type_specifiers;
