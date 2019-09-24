#pragma once

#include <string>
#include <vector>
#include <list>

#include "lex.hpp"
#include "misc.hpp"

class t_parse_error : public t_compile_error {
public:
    t_parse_error(const std::string& str, t_loc loc)
        : t_compile_error("parse error: " + str, loc) {
    }
    t_parse_error(t_loc loc)
        : t_compile_error("", loc) {
    }
};

struct t_ast {
    std::string uu;
    std::string vv;
    std::vector<t_ast> children;
    t_loc loc;

    t_ast() {}

    void add_child(const t_ast& x) {
        children.push_back(x);
    }

    t_ast(const std::string& u, const std::string& v,
          const std::vector<t_ast>& c) {
        uu = u;
        vv = v;
        children = c;
    }

    t_ast(const std::string& u) {
        uu = u;
    }

    t_ast(const std::string& u, const std::vector<t_ast>& c) {
        uu = u;
        children = c;
    }

    t_ast(const std::string& u, const std::string& v) {
        uu = u;
        vv = v;
    }

    t_ast(const std::vector<t_ast>& c) {
        children = c;
    }

    t_ast(const std::string& u, t_loc _loc) {
        uu = u;
        _loc = loc;
    }

    t_ast(const std::string& u, const std::string& v, t_loc _loc) {
        uu = u;
        vv = v;
        _loc = loc;
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

extern const std::vector<std::string> type_specifiers;
