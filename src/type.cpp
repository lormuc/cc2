#include <iostream>
#include <cassert>

#include "misc.hpp"
#include "type.hpp"

using namespace std;

const t_type char_type = t_type(t_type_kind::_char);
const t_type s_char_type = t_type(t_type_kind::_s_char);
const t_type u_char_type = t_type(t_type_kind::_u_char);
const t_type short_type = t_type(t_type_kind::_short);
const t_type u_short_type = t_type(t_type_kind::_u_short);
const t_type int_type = t_type(t_type_kind::_int);
const t_type u_int_type = t_type(t_type_kind::_u_int);
const t_type long_type = t_type(t_type_kind::_long);
const t_type u_long_type = t_type(t_type_kind::_u_long);
const t_type float_type = t_type(t_type_kind::_float);
const t_type double_type = t_type(t_type_kind::_double);
const t_type long_double_type = t_type(t_type_kind::_long_double);
const t_type void_type = t_type(t_type_kind::_void);
const t_type string_type = make_pointer_type(char_type);
const t_type void_pointer_type = make_pointer_type(void_type);

string stringify(t_type_kind k) {
    vector<string> table = {
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
        "enum",
    };
    auto ik = int(k);
    assert(ik >= 0 and ik < table.size());
    return table[ik];
}


void t_type_ptr::init(const t_type* p) {
    ptr = nullptr;
    if (p != nullptr) {
        ptr = new t_type(*p);
    }
}

t_type_ptr::~t_type_ptr() {
    delete ptr;
}

bool t_type_ptr::operator==(const t_type_ptr& x) const {
    return (ptr == nullptr and x.ptr == nullptr) or *ptr == *x;
}

t_type_ptr& t_type_ptr::operator=(const t_type_ptr& x) {
    if (this != &x) {
        delete ptr;
        init(x.ptr);
    }
    return *this;
}


t_type::t_type(t_type_kind k, const t_type& t, unsigned l) {
    if (k == t_type_kind::_array) {
        kind = k;
        array_length = l;
        subtype = new t_type(t);
    } else {
        throw std::runtime_error("bad type kind in this ctor");
    }
}

t_type::t_type(t_type_kind k, const t_type& t) {
    if (k == t_type_kind::_pointer) {
        kind = k;
        subtype = new t_type(t);
    } else if (k == t_type_kind::_array) {
        kind = k;
        subtype = new t_type(t);
        array_length = -1;
    } else {
        throw std::runtime_error("bad type kind in this ctor");
    }
}

t_type::t_type(t_type_kind k, const std::string& n) {
    if (k == t_type_kind::_struct) {
        kind = k;
        name = n;
    } else {
        throw std::runtime_error("bad type kind in this ctor");
    }
}

t_type::t_type(t_type_kind k, const std::string& n,
               const std::vector<t_struct_member>& m) {
    if (k == t_type_kind::_struct) {
        kind = k;
        name = n;
        members = m;
    } else {
        throw std::runtime_error("bad type kind in this ctor");
    }
}

t_type::t_type(t_type_kind k, const t_type& r, const std::vector<t_type>& p) {
    if (k == t_type_kind::_function) {
        kind = k;
        subtype = new t_type(r);
        params = p;
    } else {
        throw std::runtime_error("bad type kind in this ctor");
    }
}

t_type::t_type(t_type_kind k) {
    kind = k;
}

const t_type& t_type::get_return_type() const {
    return *subtype;
}

const t_type& t_type::get_pointee_type() const {
    return *subtype;
}

const t_type& t_type::get_element_type() const {
    return *subtype;
}

unsigned t_type::get_length() const {
    if (kind == t_type_kind::_array) {
        return array_length;
    } else {
        return members.size();
    }
}

unsigned t_type::get_member_index(const std::string& id) const {
    unsigned res = -1;
    unsigned i = 0;
    while (i != members.size()) {
        if (members[i].id == id) {
            res = i;
            break;
        }
        i++;
    }
    return res;
}

bool t_type::is_complete() const {
    if (kind == t_type_kind::_array) {
        return array_length != -1u;
    } else if (kind == t_type_kind::_struct or kind == t_type_kind::_union) {
        return not members.empty();
    } else if (kind == t_type_kind::_void) {
        return false;
    } else {
        return true;
    }
}

const vector<t_struct_member>& t_type::get_members() const {
    return members;
}

const vector<t_type>& t_type::get_params() const {
    return params;
}

const string& t_type::get_name() const {
    return name;
}

bool t_type::is_const() const {
    return _const;
}

bool t_type::is_volatile() const {
    return _volatile;
}

t_type_kind t_type::get_kind() const {
    return kind;
}

t_type t_type::get_member_type(unsigned i) const {
    return members[i].type;
}

t_type make_pointer_type(const t_type& t) {
    return t_type(t_type_kind::_pointer, t);
}

t_type make_array_type(const t_type& t) {
    return t_type(t_type_kind::_array, t);
}

t_type make_array_type(const t_type& t, unsigned l) {
    return t_type(t_type_kind::_array, t, l);
}

t_type make_struct_type(const string& id,
                        const vector<t_struct_member>& members) {
    return t_type(t_type_kind::_struct, id, members);
}

t_type make_struct_type(const string& id) {
    return t_type(t_type_kind::_struct, id);
}

t_type make_function_type(const t_type& r, const std::vector<t_type>& p) {
    return t_type(t_type_kind::_function, r, p);
}

bool is_pointer_type(const t_type& t) {
    return t.get_kind() == t_type_kind::_pointer;
}

bool is_array_type(const t_type& t) {
    return t.get_kind() == t_type_kind::_array;
}

bool is_function_type(const t_type& t) {
    return t.get_kind() == t_type_kind::_function;
}

bool is_struct_type(const t_type& t) {
    return t.get_kind() == t_type_kind::_struct;
}

bool is_integral_type(const t_type& t) {
    return static_cast<int>(t.get_kind()) < 9;
}

bool is_floating_type(const t_type& t) {
    auto i = static_cast<int>(t.get_kind());
    return i >= 9 and i < 12;
}

bool is_arithmetic_type(const t_type& t) {
    return is_integral_type(t.get_kind()) or is_floating_type(t.get_kind());
}

bool is_scalar_type(const t_type& t) {
    auto k = t.get_kind();
    return is_arithmetic_type(k) or k == t_type_kind::_pointer;
}

bool t_type::operator==(const t_type& x) const {
    return (kind == x.kind
            and _const == x._const
            and _volatile == x._volatile
            and subtype == x.subtype
            and name == x.name
            and array_length == x.array_length
            and members == x.members
            and params == x.params);
}

bool compatible(const t_type& x, const t_type& y) {
    auto xk = x.get_kind();
    auto yk = y.get_kind();
    if (xk == yk) {
        if (xk == t_type_kind::_pointer) {
            return (x.is_const() == y.is_const()
                    and x.is_volatile() == y.is_volatile()
                    and compatible(x.get_pointee_type(), y.get_pointee_type()));
        } else if (xk == t_type_kind::_array) {
            return (compatible(x.get_element_type(), y.get_element_type())
                    and (x.get_length() == -1u or y.get_length() == -1u
                         or x.get_length() == y.get_length()));
        } else if (xk == t_type_kind::_function) {
            auto& xp = x.get_params();
            auto& yp = y.get_params();
            if (not compatible(x.get_return_type(), y.get_return_type())
                or xp.size() != yp.size()) {
                return false;
            }
            for (auto i = 0u; i < xp.size(); i++) {
                // need to unqualify and convert to pointers
                if (not compatible(xp[i], yp[i])) {
                    return false;
                }
            }
            return true;
        } else if (xk == t_type_kind::_struct or xk == t_type_kind::_union
                   or xk == t_type_kind::_enum) {
            return x.get_name() == y.get_name();
        } else {
            return true;
        }
    }
    if ((xk == t_type_kind::_int and yk == t_type_kind::_enum)
        or (xk == t_type_kind::_enum and yk == t_type_kind::_int)) {
        return true;
    }
    return false;
}

string stringify(const t_type& type, string id) {
    string res;
    if (is_function_type(type)) {
        res += stringify(type.get_return_type());
        res += " (";
        res += id;
        res += ")(";
        auto& params = type.get_params();
        if (not params.empty()) {
            auto initial = true;
            for (auto& p : params) {
                if (not initial) {
                    res += ", ";
                }
                res += stringify(p);
                initial = false;
            }
        }
        res += ")";
    } else if (is_pointer_type(type)) {
        auto new_id = string("*") + id;
        res += stringify(type.get_pointee_type(), new_id);
    } else if (is_array_type(type)) {
        auto len = type.get_length();
        string len_str;
        if (len != -1u) {
            len_str += to_string(len);
        }
        if (id != "") {
            id = string("(") + id + ")";
        }
        auto new_id = id + "[" + len_str + "]";
        res += stringify(type.get_element_type(), new_id);
    } else {
        res += stringify(type.get_kind());
        if (id != "") {
            res += " ";
            res += id;
        }
    }
    return res;
}
