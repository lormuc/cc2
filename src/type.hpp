#pragma once

#include <vector>
#include <string>

#include <stdexcept>
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
    _bool,
};

class t_type;

class t_type_ptr {
    t_type* ptr;
    void init(const t_type* p);
public:
    t_type_ptr(const t_type* p = nullptr) {
        init(p);
    }
    t_type_ptr(const t_type_ptr& x) {
        init(x.ptr);
    }
    t_type_ptr& operator=(const t_type_ptr& x);
    ~t_type_ptr();
    const t_type& operator*() const {
        return *ptr;
    }
    bool operator==(const t_type_ptr& x) const;
    bool operator==(nullptr_t) const {
        return ptr == nullptr;
    }
};

class t_struct_member;

class t_incomplete_member_type_error
    : public std::runtime_error {
    size_t idx;
public:
    t_incomplete_member_type_error(size_t _idx)
        : std::runtime_error("member has incomplete type")
        , idx(_idx)
    {}
    size_t get_idx() { return idx; }
};

class t_type {
    t_type_kind kind;
    bool is_size_known = false;
    ull size = -1;
    ui alignment = 1;
    bool _const = false;
    bool _volatile = false;
    t_type_ptr subtype;
    std::string name;
    // array
    int array_length = -1;
    // struct or union
    std::vector<t_struct_member> members;
    // function
    std::vector<t_type> params;
public:
    t_type() {}
    t_type(t_type_kind, const t_type&, int);
    t_type(t_type_kind, const t_type&);
    t_type(t_type_kind, const std::string&);
    t_type(t_type_kind, const std::string&,
           const std::vector<t_struct_member>&);
    t_type(t_type_kind, const t_type&, const std::vector<t_type>&);
    t_type(t_type_kind);
    void set_size(ull, ui);
    bool is_const() const;
    bool is_volatile() const;
    t_type_kind get_kind() const;
    bool get_is_size_known() const;
    ull get_size() const;
    ui get_alignment() const;
    const t_type& get_return_type() const;
    const t_type& get_pointee_type() const;
    const t_type& get_element_type() const;
    int get_length() const;
    bool has_unknown_length() const;
    int get_member_index(const std::string&) const;
    t_type get_member_type(int i) const;
    bool is_complete() const;
    const std::string& get_name() const;
    const std::vector<t_struct_member>& get_members() const;
    const std::vector<t_type>& get_params() const;
    bool operator==(const t_type&) const;
};

struct t_struct_member {
    std::string id;
    t_type type;
    bool operator==(const t_struct_member& x) const {
        return id == x.id and type == x.type;
    }
};

t_type make_function_type(const t_type&, const std::vector<t_type>&);
t_type make_basic_type(const std::string&);
t_type make_pointer_type(const t_type&);
t_type make_array_type(const t_type&);
t_type make_array_type(const t_type&, unsigned);
t_type make_struct_type(const std::string&,
                        const std::vector<t_struct_member>&);
t_type make_struct_type(const std::string&);
bool is_array_type(const t_type&);
bool is_function_type(const t_type&);
bool is_struct_type(const t_type&);
bool is_pointer_type(const t_type&);
bool is_integral_type(const t_type&);
bool is_floating_type(const t_type&);
bool is_arithmetic_type(const t_type&);
bool is_scalar_type(const t_type&);
bool is_signed_integer_type(const t_type&);
bool is_unsigned_integer_type(const t_type&);
bool compatible(const t_type&, const t_type&);
std::string stringify(const t_type&, std::string id = "");

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
extern const t_type bool_type;
