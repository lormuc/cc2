#include <iostream>
#include <cassert>

#include "misc.hpp"
#include "type.hpp"

const t_type char_type(t_type_kind::_char);
const t_type u_char_type(t_type_kind::_u_char);
const t_type s_char_type(t_type_kind::_s_char);
const t_type short_type(t_type_kind::_short);
const t_type u_short_type(t_type_kind::_u_short);
const t_type int_type(t_type_kind::_int);
const t_type u_int_type(t_type_kind::_u_int);
const t_type long_type(t_type_kind::_long);
const t_type u_long_type(t_type_kind::_u_long);
const t_type float_type(t_type_kind::_float);
const t_type double_type(t_type_kind::_double);
const t_type long_double_type(t_type_kind::_long_double);
const t_type void_type(t_type_kind::_void);
const t_type void_pointer_type(t_type_kind::_pointer, void_type);
const t_type uintptr_t_type = u_long_type;
const t_type size_t_type = u_long_type;
const t_type ptrdiff_t_type = long_type;
const t_type string_type(t_type_kind::_pointer, char_type);

t_type::t_type(t_type_kind k, t_type t, size_t len)
    : ptr(new t_type_aux) {
    assert(k == t_type_kind::_array);
    (*ptr).kind = k;
    (*ptr).size = len * t.size();
    (*ptr).alignment = t.alignment();
    (*ptr).length = len;
    (*ptr).children.push_back(t);
}

t_type::t_type(t_type_kind k, t_type t)
    : ptr(new t_type_aux) {
    assert(k == t_type_kind::_array or k == t_type_kind::_pointer);
    (*ptr).kind = k;
    if (k == t_type_kind::_pointer) {
        set_size(8);
        (*ptr).children.push_back(t);
    } else {
        (*ptr).children.push_back(t);
    }
}

t_type::t_type(t_type_kind k, const str& n)
    : ptr(new t_type_aux) {
    assert(k == t_type_kind::_enum);
    (*ptr).kind = k;
    set_size(4);
    (*ptr).name = n;
}

t_type::t_type(t_type_kind kind, const str& name,
               vec<str> field_names, vec<t_type> field_types, const str& as)
    : ptr(new t_type_aux) {
    assert(kind == t_type_kind::_struct);
    _ len = field_types.size();
    size_t _alignment = 1;
    size_t _size = 0;
    for (size_t i = 0; i < len; i++) {
        assert(field_types[i].is_complete());
        _size += field_types[i].size();
        _alignment = std::max(_alignment, field_types[i].alignment());
        _ al = ((i+1 < len) ? field_types[i+1].alignment() : _alignment);
        if ((_size % al) != 0) {
            _size += al - (_size % al);
        }
    }
    (*ptr).kind = kind;
    (*ptr).alignment = _alignment;
    (*ptr).size = _size;
    (*ptr).name = name;
    (*ptr).field_names = std::move(field_names);
    (*ptr).children = std::move(field_types);
    (*ptr).as = as;
}

t_type::t_type(t_type_kind k, t_type r, vec<t_type> p, bool v)
    : ptr(new t_type_aux) {
    assert(k == t_type_kind::_function);
    (*ptr).kind = k;
    (*ptr).children.push_back(r);
    (*ptr).params = std::move(p);
    (*ptr).is_variadic = v;
}

t_type::t_type(t_type_kind k)
    : ptr(new t_type_aux) {
    (*ptr).kind = k;
    if (k == t_type_kind::_char or k == t_type_kind::_s_char
        or k == t_type_kind::_u_char) {
        set_size(1);
    } else if (k == t_type_kind::_short or k == t_type_kind::_u_short) {
        set_size(2);
    } else if (k == t_type_kind::_int or k == t_type_kind::_u_int) {
        set_size(4);
    } else if (k == t_type_kind::_long or k == t_type_kind::_u_long) {
        set_size(8);
    } else if (k == t_type_kind::_float) {
        set_size(4);
    } else if (k == t_type_kind::_double) {
        set_size(8);
    } else if (k == t_type_kind::_long_double) {
        set_size(8);
    }
}

t_type::t_type(t_type type, int) {
    if (type.is_const() or type.is_volatile()) {
        ptr.reset(new t_type_aux(*type.ptr));
        (*ptr).is_const = false;
        (*ptr).is_volatile = false;
    } else {
        ptr = type.ptr;
    }
}

t_type unqualify(t_type t) {
    return t_type(t, int());
}

void t_type::set_size(size_t s) {
    (*ptr).size = s;
    (*ptr).alignment = s;
}

const vec<str>& t_type::field_names() const {
    return (*ptr).field_names;
}

t_type t_type::return_type() const {
    assert(kind() == t_type_kind::_function);
    return (*ptr).children[0];
}

t_type t_type::pointee_type() const {
    assert(kind() == t_type_kind::_pointer);
    return (*ptr).children[0];
}

t_type t_type::element_type() const {
    assert(kind() == t_type_kind::_array);
    return (*ptr).children[0];
}

t_type t_type::element_type(size_t idx) const {
    if (is_array()) {
        return element_type();
    } else {
        return (*ptr).children[idx];
    }
}

size_t t_type::length() const {
    if (kind() == t_type_kind::_array) {
        return (*ptr).length;
    } else {
        return (*ptr).children.size();
    }
}

size_t t_type::field_index(const str& name) const {
    for (size_t i = 0; i < field_names().size(); i++) {
        if (field_names()[i] == name) {
            return i;
        }
    }
    return -1;
}

bool t_type::operator==(t_type x) const {
    return *ptr == *x.ptr;
}

bool t_type::operator!=(t_type x) const {
    return *ptr != *x.ptr;
}

const vec<t_type>& t_type::fields() const {
    return (*ptr).children;
}

const vec<t_type>& t_type::params() const {
    return (*ptr).params;
}

const str& t_type::name() const {
    return (*ptr).name;
}

bool t_type::is_const() const {
    return (*ptr).is_const;
}

bool t_type::is_volatile() const {
    return (*ptr).is_volatile;
}

t_type_kind t_type::kind() const {
    return (*ptr).kind;
}

size_t t_type::size() const {
    return (*ptr).size;
}

size_t t_type::alignment() const {
    return (*ptr).alignment;
}

t_type t_type::field(size_t i) const {
    assert(i < (*ptr).children.size());
    return (*ptr).children[i];
}

t_type make_pointer_type(t_type t) {
    return t_type(t_type_kind::_pointer, t);
}

t_type make_array_type(t_type t) {
    return t_type(t_type_kind::_array, t);
}

t_type make_array_type(t_type t, size_t l) {
    return t_type(t_type_kind::_array, t, l);
}

t_type make_struct_type(const str& name, vec<str> field_names,
                        vec<t_type> field_types, const str& _as) {
    return t_type(t_type_kind::_struct, name, std::move(field_names),
                  std::move(field_types), _as);
}

t_type make_struct_type(const str& name, const str& _as) {
    return t_type(t_type_kind::_struct, name, {}, {}, _as);
}

t_type make_enum_type(const str& name) {
    return t_type(t_type_kind::_enum, name);
}

t_type make_func_type(t_type r, vec<t_type> p, bool is_variadic) {
    return t_type(t_type_kind::_function, r, std::move(p), is_variadic);
}

bool t_type::is_pointer() const {
    return kind() == t_type_kind::_pointer;
}

bool t_type::is_variadic() const {
    return (*ptr).is_variadic;
}

bool t_type::is_array() const {
    return kind() == t_type_kind::_array;
}

bool t_type::is_function() const {
    return kind() == t_type_kind::_function;
}

bool t_type::is_struct() const {
    return kind() == t_type_kind::_struct;
}

bool t_type::is_enum() const {
    return kind() == t_type_kind::_enum;
}

bool t_type::is_union() const {
    return kind() == t_type_kind::_union;
}

bool t_type::is_integral() const {
    return is_integer() or kind() == t_type_kind::_char or is_enum();
}

bool t_type::is_integer() const {
    return is_signed_integer() or is_unsigned_integer();
}

bool t_type::is_floating() const {
    return (kind() == t_type_kind::_float or kind() == t_type_kind::_double
            or kind() == t_type_kind::_long_double);
}

bool t_type::is_arithmetic() const {
    return is_integral() or is_floating();
}

bool t_type::is_scalar() const {
    return is_arithmetic() or is_pointer();
}

bool t_type::is_signed_integer() const {
    return (kind() == t_type_kind::_int
            or kind() == t_type_kind::_s_char
            or kind() == t_type_kind::_short
            or kind() == t_type_kind::_long);
}

bool t_type::is_unsigned_integer() const {
    return (kind() == t_type_kind::_u_int
            or kind() == t_type_kind::_u_char
            or kind() == t_type_kind::_u_short
            or kind() == t_type_kind::_u_long);
}

bool t_type::is_signed() const {
    return is_signed_integer() or is_enum();
}

bool t_type::is_unsigned() const {
    return is_unsigned_integer() or kind() == t_type_kind::_char;
}

bool t_type::is_object() const {
    return not is_function();
}

bool t_type::is_complete() const {
    return size() != 0;
}

bool t_type::is_incomplete() const {
    return size() == 0;
}

bool t_type::is_pointer_to_object() const {
    return is_pointer() and pointee_type().is_object();
}

bool t_type::has_known_length() const {
    return length() != 0;
}

str t_type::as(bool expand) const {
    _& t = *this;
    str res;
    if (t == void_type) {
        res = "void";
    } else if (t == char_type or t == u_char_type
        or t == s_char_type) {
        res = "i8";
    } else if (t == short_type or t == u_short_type) {
        res = "i16";
    } else if (t == int_type or t == u_int_type) {
        res = "i32";
    } else if (t == long_type or t == u_long_type) {
        res = "i64";
    } else if (t == float_type) {
        res = "float";
    } else if (t == double_type) {
        res = "double";
    } else if (t == long_double_type) {
        res = "double";
    } else if (t == void_pointer_type) {
        res += "i8*";
    } else if (t.is_pointer()) {
        res += t.pointee_type().as();
        res += "*";
    } else if (t.is_array()) {
        res += "[";
        res += std::to_string(t.length()) + " x ";
        res += t.element_type().as();
        res += "]";
    } else if (t.is_struct()) {
        if (expand) {
            res += "{ ";
            _ start = true;
            for (_& field : t.fields()) {
                if (not start) {
                    res += ", ";
                }
                start = false;
                res += field.as();
            }
            res += " }";
        } else {
            res += (*ptr).as;
        }
    } else if (t.is_enum()) {
        res += "i32";
    } else if (t.is_function()) {
        res += t.return_type().as() + " (";
        _ start = true;
        for (_& param : t.params()) {
            if (not start) {
                res += ", ";
            }
            start = false;
            res += param.as();
        }
        if (t.is_variadic()) {
            res += ", ...";
        }
        res += ")";
    } else {
        throw std::logic_error("type.as (" + stringify(t) + ") failure");
    }
    return res;
}

bool compatible(t_type x, t_type y) {
    if (x.kind() == y.kind()) {
        if (x.is_pointer()) {
            return (x.is_const() == y.is_const()
                    and x.is_volatile() == y.is_volatile()
                    and compatible(x.pointee_type(), y.pointee_type()));
        } else if (x.is_array()) {
            return (compatible(x.element_type(), y.element_type())
                    and (x.is_incomplete() or y.is_incomplete()
                         or x.length() == y.length()));
        } else if (x.is_function()) {
            if (not (compatible(x.return_type(), y.return_type())
                     and x.params().size() == y.params().size()
                     and x.is_variadic() == y.is_variadic())) {
                return false;
            }
            for (size_t i = 0; i < x.params().size(); i++) {
                if (not compatible(x.params()[i], y.params()[i])) {
                    return false;
                }
            }
            return true;
        } else if (x.is_struct() or x.is_enum() or x.is_union()) {
            return x.name() == y.name();
        } else {
            return true;
        }
    }
    if ((x == int_type and y.is_enum()) or (x.is_enum() and y == int_type)) {
        return true;
    }
    return false;
}

const vec<str> stringify_table = {
    "char",
    "signed char",
    "unsigned char",
    "short",
    "unsigned short",
    "int",
    "unsigned int",
    "long",
    "unsigned long",
    "float",
    "double",
    "long double",
    "void",
    "array",
    "pointer",
    "struct",
    "union",
    "function",
    "enum"
};

str stringify(t_type_kind k) {
    _ ik = size_t(k);
    assert(ik < stringify_table.size());
    return stringify_table[ik];
}

str stringify(t_type type, str name) {
    str res;
    if (type.is_function()) {
        res += stringify(type.return_type());
        res += " (";
        res += name;
        res += ")(";
        if (not type.params().empty()) {
            _ initial = true;
            for (_& p : type.params()) {
                if (not initial) {
                    res += ", ";
                }
                res += stringify(p);
                initial = false;
            }
        }
        res += ")";
    } else if (type.is_pointer()) {
        _ new_name = "*" + name;
        res += stringify(type.pointee_type(), new_name);
    } else if (type.is_array()) {
        str len_str;
        if (type.is_complete()) {
            len_str += std::to_string(type.length());
        }
        if (name != "") {
            name = "(" + name + ")";
        }
        _ new_name = name + "[" + len_str + "]";
        res += stringify(type.element_type(), new_name);
    } else {
        res += stringify(type.kind());
        if (name != "") {
            res += " ";
            res += name;
        }
    }
    return res;
}
