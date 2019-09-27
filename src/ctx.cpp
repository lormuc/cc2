#include <string>

#include "val.hpp"
#include "ctx.hpp"

void t_ctx::enter_switch() {
    cases.clear();
    case_vals.clear();
    case_idx = 0;
    _default_label = "";
}

void t_ctx::def_case(const t_val& v, const str& l) {
    if (case_vals.count(v.u_val()) == 0) {
        case_vals.insert(v.u_val());
        cases.push_back({asmv(v), l});
    } else {
        throw t_redefinition_error();
    }
}

str t_ctx::get_case_label() {
    auto& res = cases[case_idx].label;
    case_idx++;
    return res;
}

str t_ctx::asmt(const t_type& t, bool expand) const {
    str res;
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
    } else if (t.is_pointer()) {
        res += asmt(t.pointee_type());
        res += "*";
    } else if (t.is_array()) {
        res += "[";
        res += std::to_string(t.length()) + " x ";
        res += asmt(t.element_type());
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
                res += asmt(field);
            }
            res += " }";
        } else {
            res += get_type_data(t.name()).asm_id;
        }
    } else if (t.is_enum()) {
        res += "i32";
    } else {
        throw std::logic_error("asmt " + stringify(t) + " fail");
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
    if (t.is_array()) {
        _ et = complete_type(t.element_type());
        return make_array_type(et, t.length());
    }
    if (not t.is_struct()) {
        return t;
    }
    _ fields = t.fields();
    if (fields.empty()) {
        return get_type_data(t.name()).type;
    }
    for (_& field : fields) {
        field = complete_type(field);
    }
    return make_struct_type(t.name(), t.field_names(), std::move(fields));
}

void t_ctx::enter_scope() {
    types.enter_scope();
    vars.enter_scope();
}
