#pragma once

#include <string>
#include <unordered_map>
#include <set>

#include "prog.hpp"
#include "val.hpp"

class t_undefined_name_error {};
class t_redefinition_error {};

template <class t>
class t_namespace {
    std::unordered_map<str, t> outer_scope;
    std::unordered_map<str, t> scope;
public:
    const t& scope_get(const str& name) const {
        auto it = scope.find(name);
        if (it != scope.end()) {
            return (*it).second;
        } else {
            throw t_undefined_name_error();
        }
    }
    const t& get(const str& name) const {
        try {
            return scope_get(name);
        } catch (t_undefined_name_error) {
            auto oit = outer_scope.find(name);
            if (oit != outer_scope.end()) {
                return (*oit).second;
            } else {
                throw;
            }
        }
    }
    void def(const str& name, const t& data) {
        auto it = scope.find(name);
        if (it != scope.end()) {
            throw t_redefinition_error();
        }
        scope[name] = data;
    }
    void put(const str& name, const t& data) {
        scope[name] = data;
    }
    void enter_scope() {
        for (const auto& [name, data] : scope) {
            outer_scope[name] = data;
        }
        scope.clear();
    }
};

struct t_type_data {
    t_type type;
    str asm_id;
};

class t_ctx {
    t_namespace<t_type_data> types;
    t_namespace<t_val> vars;
    t_namespace<str> labels;
    str _loop_body_end;
    str _break_label;
    str _func_name;
    vec<t_asm_case> cases;
    std::set<unsigned long> case_vals;
    int case_idx = -1;
    str _default_label;
public:
    void default_label(const str& l) {
        _default_label = l;
    }
    const str& default_label() {
        return _default_label;
    }
    void enter_switch();
    void def_case(const t_val& v, const str& l);
    str get_case_label();
    vec<t_asm_case> get_asm_cases() {
        return cases;
    }
    const str& get_label_data(const str& name) const {
        return labels.get(name);
    }
    void def_label(const str& name, const str& data) {
        labels.def(name, data);
    }
    const t_val& get_var_data(const str& name) const {
        return vars.get(name);
    }
    void def_var(const str& name, const t_val& data) {
        vars.def(name, data);
    }
    const t_type_data& get_type_data(const str& name) const {
        return types.get(name);
    }
    const t_type_data& scope_get_type_data(const str& name) const {
        return types.scope_get(name);
    }
    void def_enum(const str& name, t_type type) {
        types.def(name, {type, ""});
    }
    void put_struct(const str& name, const t_type_data& data) {
        types.put(name, data);
    }

    auto loop_body_end(const str& x) {
        _loop_body_end = x;
    }
    const auto& loop_body_end() {
        return _loop_body_end;
    }
    auto break_label(const str& x) {
        _break_label = x;
    }
    const auto& break_label() {
        return _break_label;
    }
    auto func_name(const str& x) {
        _func_name = x;
    }
    const auto& func_name() {
        return _func_name;
    }

    t_type complete_type(const t_type& t) const;
    str asmt(const t_type& t, bool expand = false) const;
    t_asm_val asmv(const t_val& val) const;
    void enter_scope();
};

