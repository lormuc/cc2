#include <string>

#include "val.hpp"
#include "ctx.hpp"

using namespace std;

void t_ctx::enter_switch() {
    cases.clear();
    case_vals.clear();
    case_idx = 0;
    _default_label = "";
}

void t_ctx::def_case(const t_val& v, const std::string& l) {
    if (case_vals.count(v.u_val()) == 0) {
        case_vals.insert(v.u_val());
        cases.push_back({asmv(v), l});
    } else {
        throw t_redefinition_error();
    }
}

std::string t_ctx::get_case_label() {
    auto& res = cases[case_idx].label;
    case_idx++;
    return res;
}

string t_ctx::asmt(const t_type& t, bool expand) const {
    string res;
    if (t == char_type or t == u_char_type
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
    } else if (is_pointer_type(t)) {
        res += asmt(t.get_pointee_type());
        res += "*";
    } else if (is_array_type(t)) {
        res += "[";
        res += to_string(t.get_length()) + " x ";
        res += asmt(t.get_element_type());
        res += "]";
    } else if (is_struct_type(t)) {
        if (expand) {
            res += "{ ";
            _ start = true;
            for (_& m : t.get_members()) {
                if (not start) {
                    res += ", ";
                }
                start = false;
                res += asmt(m.type);
            }
            res += " }";
        } else {
            res += get_type_data(t.get_name()).asm_id;
        }
    } else if (is_enum_type(t)) {
        res += "i32";
    } else {
        throw logic_error("asmt " + stringify(t) + " fail");
    }
    return res;
}

t_asm_val t_ctx::asmv(const t_val& val) const {
    _ type = asmt(val.type());
    if (val.is_lvalue()) {
        type += "*";
    }
    return t_asm_val{type, val.asm_id()};
}

t_type t_ctx::complete_type(const t_type& t) const {
    if (is_array_type(t)) {
        _ et = complete_type(t.get_element_type());
        return make_array_type(et, t.get_length());
    }
    if (not is_struct_type(t)) {
        return t;
    }
    if (not t.is_complete()) {
        return get_type_data(t.get_name()).type;
    }
    _ mm = t.get_members();
    for (_& m : mm) {
        m.type = complete_type(m.type);
    }
    return make_struct_type(t.get_name(), mm);
}

void t_ctx::enter_scope() {
    types.enter_scope();
    vars.enter_scope();
}
