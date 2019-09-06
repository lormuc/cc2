#pragma once

#include <vector>
#include <string>

#include "ast.hpp"
#include "misc.hpp"

typedef t_ast t_type;

struct t_struct_member {
    std::string id;
    t_type type;
};

t_type make_basic_type(const std::string&);
t_type make_pointer_type(const t_type&);
t_type pointer_get_referenced_type(const t_type&);
t_type make_array_type(const t_type&);
t_type make_array_type(const t_type&, ull);
const t_type& array_get_element_type(const t_type&);
t_type& array_get_element_type(t_type&);
ull array_get_length(const t_type&);
bool is_array_type(const t_type&);
bool is_pointer_type(const t_type&);
bool is_arithmetic_type(const t_type&);
bool is_integral_type(const t_type&);
bool is_floating_type(const t_type&);
bool is_scalar_type(const t_type&);
bool struct_is_complete(const t_type&);
size_t struct_get_member_idx(const t_type&, const std::string&);
t_type& struct_get_member_type(t_type&, size_t);
const t_type& struct_get_member_type(const t_type&, size_t);
std::string struct_get_name(const t_type&);
std::vector<t_struct_member> struct_get_members(const t_type&);
t_type make_struct_type(const std::string&,
                        const std::vector<t_struct_member>&);
t_type make_struct_type(const std::string&);
bool is_struct_type(const t_type&);
bool equal(const t_type&, const t_type&);

const auto signed_char_type = t_type("signed char");
const auto unsigned_char_type = t_type("unsigned char");
const auto short_type = t_type("short int");
const auto unsigned_short_type = t_type("unsigned short int");
const auto int_type = t_type("int");
const auto unsigned_type = t_type("unsigned int");
const auto long_type = t_type("long int");
const auto unsigned_long_type = t_type("unsigned long int");
const auto char_type = t_type("char");
const auto float_type = t_type("float");
const auto double_type = t_type("double");
const auto long_double_type = t_type("long double");
const auto void_type = t_type("void");
const auto string_type = make_pointer_type(char_type);
const auto void_pointer_type = make_pointer_type(void_type);
