#pragma once

#include <vector>
#include <string>
#include <stdexcept>
#include <memory>
#include <unordered_map>

#include "misc.hpp"

enum class t_type_kind {
    _char,
    _s_char,
    _u_char,
    _short,
    _u_short,
    _int,
    _u_int,
    _long,
    _u_long,
    _float,
    _double,
    _long_double,
    _void,
    _array,
    _pointer,
    _struct,
    _union,
    _function,
    _enum,
};

class t_type {
    struct t_type_aux {
        t_type_kind kind;
        size_t size = 0;
        size_t alignment = 1;
        size_t length = 0;
        bool is_const = false;
        bool is_volatile = false;
        bool is_variadic = false;
        str name;
        str as;
        vec<str> field_names;
        vec<t_type> params;
        vec<t_type> children;
        bool operator==(const t_type_aux& x) const {
            return (kind == x.kind
                    and size == x.size
                    and alignment == x.alignment
                    and length == x.length
                    and is_const == x.is_const
                    and is_volatile == x.is_volatile
                    and name == x.name
                    and field_names == x.field_names
                    and children == x.children);
        }
        bool operator!=(const t_type_aux& x) const {
            return not (*this == x);
        }
    };
    std::shared_ptr<t_type_aux> ptr;
    void set_size(size_t);
public:
    static const size_t bad_field_index = -1;
    t_type() {}
    t_type(t_type_kind, t_type, size_t);
    t_type(t_type_kind, t_type);
    t_type(t_type_kind, const str&);
    t_type(t_type_kind, const str&, vec<str>, vec<t_type>, const str&);
    t_type(t_type_kind, t_type, vec<t_type>, bool);
    t_type(t_type_kind);
    t_type(t_type, int);
    bool is_const() const;
    bool is_volatile() const;
    t_type_kind kind() const;
    size_t size() const;
    size_t alignment() const;
    t_type return_type() const;
    t_type pointee_type() const;
    t_type element_type() const;
    t_type element_type(size_t) const;
    size_t length() const;
    size_t field_index(const str&) const;
    t_type field(size_t) const;
    const str& name() const;
    const vec<t_type>& fields() const;
    const vec<t_type>& params() const;
    const vec<str>& field_names() const;
    bool operator==(t_type) const;
    bool operator!=(t_type) const;
    bool is_array() const;
    bool is_function() const;
    bool is_struct() const;
    bool is_enum() const;
    bool is_union() const;
    bool is_pointer() const;
    bool is_integral() const;
    bool is_floating() const;
    bool is_arithmetic() const;
    bool is_scalar() const;
    bool is_signed_integer() const;
    bool is_unsigned_integer() const;
    bool is_signed() const;
    bool is_unsigned() const;
    bool is_integer() const;
    bool is_object() const;
    bool is_complete() const;
    bool is_incomplete() const;
    bool is_pointer_to_object() const;
    bool is_variadic() const;
    str as(bool = false) const;
};

t_type make_func_type(t_type, vec<t_type>, bool = false);
t_type make_basic_type(const str&);
t_type make_pointer_type(t_type);
t_type make_array_type(t_type);
t_type make_array_type(t_type, size_t);
t_type make_struct_type(const str&, vec<str>, vec<t_type>, const str&);
t_type make_struct_type(const str&, const str&);
t_type make_enum_type(const str&);
t_type unqualify(t_type);
bool compatible(t_type, t_type);
str stringify(t_type, str name = "");

extern const t_type char_type;
extern const t_type s_char_type;
extern const t_type u_char_type;
extern const t_type short_type;
extern const t_type u_short_type;
extern const t_type int_type;
extern const t_type u_int_type;
extern const t_type long_type;
extern const t_type u_long_type;
extern const t_type float_type;
extern const t_type double_type;
extern const t_type long_double_type;
extern const t_type void_type;
extern const t_type string_type;
extern const t_type void_pointer_type;
extern const t_type uintptr_t_type;
extern const t_type size_t_type;
extern const t_type ptrdiff_t_type;
extern const t_type string_type;
