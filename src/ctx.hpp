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
    std::unordered_map<std::string, t> outer_scope;
    std::unordered_map<std::string, t> scope;
public:
    const t& scope_get(const std::string& name) const {
        auto it = scope.find(name);
        if (it != scope.end()) {
            return (*it).second;
        } else {
            throw t_undefined_name_error();
        }
    }
    const t& get(const std::string& name) const {
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
    void def(const std::string& name, const t& data) {
        auto it = scope.find(name);
        if (it != scope.end()) {
            throw t_redefinition_error();
        }
        scope[name] = data;
    }
    void put(const std::string& name, const t& data) {
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
    std::string asm_id;
};

class t_ctx {
    t_namespace<t_type_data> types;
    t_namespace<t_val> vars;
    t_namespace<std::string> labels;
    std::string _loop_body_end;
    std::string _break_label;
    std::string _func_name;
    std::vector<t_asm_case> cases;
    std::set<unsigned long> case_vals;
    int case_idx = -1;
    std::string _default_label;
public:
    void default_label(const std::string& l) {
        _default_label = l;
    }
    const std::string& default_label() {
        return _default_label;
    }
    void enter_switch();
    void def_case(const t_val& v, const std::string& l);
    std::string get_case_label();
    std::vector<t_asm_case> get_asm_cases() {
        return cases;
    }
    const std::string& get_label_data(const std::string& name) const {
        return labels.get(name);
    }
    void def_label(const std::string& name, const std::string& data) {
        labels.def(name, data);
    }
    const t_val& get_var_data(const std::string& name) const {
        return vars.get(name);
    }
    void def_var(const std::string& name, const t_val& data) {
        vars.def(name, data);
    }
    const t_type_data& get_type_data(const std::string& name) const {
        return types.get(name);
    }
    const t_type_data& scope_get_type_data(const std::string& name) const {
        return types.scope_get(name);
    }
    void def_type(const std::string& name, const t_type_data& data) {
        types.def(name, data);
    }
    void put_struct(const std::string& name, const t_type_data& data) {
        types.put(name, data);
    }

    auto loop_body_end(const std::string& x) {
        _loop_body_end = x;
    }
    const auto& loop_body_end() {
        return _loop_body_end;
    }
    auto break_label(const std::string& x) {
        _break_label = x;
    }
    const auto& break_label() {
        return _break_label;
    }
    auto func_name(const std::string& x) {
        _func_name = x;
    }
    const auto& func_name() {
        return _func_name;
    }

    t_type complete_type(const t_type& t) const;
    std::string asmt(const t_type& t, bool expand = false) const;
    t_asm_val asmv(const t_val& val) const;
    void enter_scope();
};

