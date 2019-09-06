#include <iostream>

#include "misc.hpp"
#include "type.hpp"

using namespace std;

t_type make_basic_type(const string& t) {
    return t_ast(t);
}

t_type make_pointer_type(const t_type& t) {
    return t_ast("pointer", {t});
}

t_type make_array_type(const t_type& t) {
    return t_ast("array", {t});
}

t_type make_array_type(const t_type& t, ull l) {
    return t_ast("array", {t, t_ast(to_string(l))});
}

t_type make_struct_type(const string& id,
                        const vector<t_struct_member>& members) {
    vector<t_type> y;
    for (auto& p : members) {
        y.push_back(t_ast(p.id, {p.type}));
    }
    return t_ast("struct", id, y);
}

t_type make_struct_type(const string& id) {
    return t_ast("struct", id);
}

vector<t_struct_member> struct_get_members(const t_type& t) {
    vector<t_struct_member> res;
    for (auto& c : t.children) {
        res.push_back({c.uu, c[0]});
    }
    return res;
}

string struct_get_name(const t_type& t) {
    return t.vv;
}

size_t struct_get_member_idx(const t_type& t, const string& id) {
    size_t res = -1;
    size_t i = 0;
    while (i != t.children.size()) {
        if (t[i].uu == id) {
            res = i;
            break;
        }
        i++;
    }
    return res;
}

const t_type& struct_get_member_type(const t_type& t, size_t i) {
    return t[i][0];
}

t_type& struct_get_member_type(t_type& t, size_t i) {
    return t[i][0];
}

bool struct_is_complete(const t_type& t) {
    return t.children.size() != 0;
}

const t_type& array_get_element_type(const t_type& t) {
    return t[0];
}

t_type& array_get_element_type(t_type& t) {
    return t[0];
}

ull array_get_length(const t_type& t) {
    return stoull(t[1].uu);
}

bool is_array_type(const t_type& t) {
    return t.uu == "array";
}

bool is_pointer_type(const t_type& t) {
    return t.uu == "pointer";
}

bool is_struct_type(const t_type& t) {
    return t.uu == "struct";
}

bool is_integral_type(const t_type& t) {
    return t == char_type or t == signed_char_type or t == short_type
        or t == int_type or t == long_type or t == unsigned_char_type
        or t == unsigned_short_type or t == unsigned_type
        or t == unsigned_long_type;
}

bool is_floating_type(const t_type& t) {
    return t == double_type or t == float_type or t == long_double_type;
}

bool is_arithmetic_type(const t_type& t) {
    return is_integral_type(t) or is_floating_type(t);
}

bool is_scalar_type(const t_type& t) {
    return is_arithmetic_type(t) or is_pointer_type(t);
}

t_type pointer_get_referenced_type(const t_type& t) {
    return t[0];
}

bool equal(const t_type& ta, const t_type& tb) {
    if (is_struct_type(ta) and is_struct_type(tb)) {
        return struct_get_name(ta) == struct_get_name(tb);
    }
    if (is_pointer_type(ta) and is_pointer_type(tb)) {
        return equal(pointer_get_referenced_type(ta),
                     pointer_get_referenced_type(tb));
    }
    return ta == tb;
}
