#pragma once

#include <string>
#include <vector>

#include "lex.hpp"
#include "misc.hpp"

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

t_ast parse_program(std::vector<t_lexeme>&);

extern const std::vector<std::string> type_specifiers;
