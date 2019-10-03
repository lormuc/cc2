#pragma once

#include <string>
#include <unordered_map>
#include <set>

#include "prog.hpp"
#include "val.hpp"

class t_undefined_name_error {};
class t_redefinition_error {};

template <class t>
class t_id_namespace {
    const t_id_namespace& _root;
    const t_id_namespace& parent;
    std::unordered_map<str, t> scope;
public:
    t_id_namespace(const t_id_namespace& _parent)
        : _root(_parent._root)
        , parent(_parent) {
    }
    t_id_namespace()
        : _root(*this)
        , parent(*this) {
    }
    const t_id_namespace& root() const {
        return _root;
    }
    bool is_root() const {
        return &parent == this;
    }
    const t& scope_get(const str& name) const {
        auto it = scope.find(name);
        if (it != scope.end()) {
            return (*it).second;
        } else {
            throw t_undefined_name_error();
        }
    }
    const std::unordered_map<str, t>& scope_get() const {
        return scope;
    }
    const t& get(const str& name) const {
        try {
            return scope_get(name);
        } catch (t_undefined_name_error) {
            if (is_root()) {
                throw;
            } else {
                return parent.get(name);
            }
        }
    }
    void put(const str& name, const t& data) {
        scope[name] = data;
    }
};

struct t_tag_data {
    t_type type;
    str as;
};

enum class t_linkage { external, internal, none };

struct t_id_data {
    t_val val;
    t_linkage linkage;
};

class t_ctx {
    t_id_namespace<t_tag_data> tags;
    t_id_namespace<t_id_data> ids;
    t_id_namespace<str> labels;
    str _loop_body_end;
    str _break_label;
    str _func_end;
    t_val _return_var;
    vec<t_asm_case> cases;
    std::set<unsigned long> case_vals;
    int case_idx = -1;
    str _default_label;
public:
    bool is_global() const { return ids.is_root(); }
    void default_label(const str& l) { _default_label = l; }
    const str& default_label() { return _default_label; }
    void enter_switch();
    void def_case(const t_val& v, const str& l);
    str get_case_label();
    void def_id(const str& name, const t_val& val);
    vec<t_asm_case> get_asm_cases() { return cases; }
    const str& get_label_data(const str& name) const {
        return labels.get(name);
    }
    void def_label(const str& name, const str& data);
    t_id_data get_id_data(const str& name) const;
    t_id_data get_global_id_data(const str&) const;
    const t_tag_data& get_tag_data(const str& name) const {
        return tags.get(name);
    }
    const t_tag_data& scope_get_tag_data(const str& name) const {
        return tags.scope_get(name);
    }
    void def_enum(const str& name, t_type type);
    void put_struct(const str& name, const t_tag_data& data) {
        tags.put(name, data);
    }
    void put_id(const str& name, const t_val& val,
                t_linkage linkage = t_linkage::none) {
        ids.put(name, {val, linkage});
    }

    _ loop_body_end(const str& x) { _loop_body_end = x; }
    const _& loop_body_end() { return _loop_body_end; }
    _ break_label(const str& x) { _break_label = x; }
    const _& break_label() { return _break_label; }
    _ func_end(const str& x) { _func_end = x; }
    const _& func_end() { return _func_end; }
    _ return_var(const t_val& x) { _return_var = x; }
    const _& return_var() { return _return_var; }

    t_type complete_type(const t_type& t) const;
    t_asm_val as(const t_val& val) const;
    str as(const str&) const;
    ~t_ctx();
};
