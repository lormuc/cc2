#pragma once

#include <string>
#include <vector>

#include "misc.hpp"

struct t_asm_val {
    str type;
    str name;

    str join() const {
        return type + " " + name;
    }
};

struct t_asm_case {
    t_asm_val val;
    str label;
};

class t_prog {
    bool _silence = false;
    int label_cnt = 0;
    int id_cnt = 0;
    str asm_funcs;
    str global_storage;
    str asm_type_defs;
    str func_body;
    str func_var_alloc;
    str _func_name;
    str _func_return_type;
    vec<str> func_params;
    bool _func_internal = false;
    str decls;

    void a(const str&);
    str aa(const str&);
    void append(str&, const str&);

public:
    str def_str(const str& str);
    str make_new_id();
    str make_new_global_id();
    void def_struct(const str& name, const str& type);
    void def_opaque_struct(const str& name);
    str def_on_stack(const str& type);
    str assemble();
    void put_label(const str&, bool = true);
    str make_label();
    void cond_br(const str&, const str&, const str&);
    void br(const str&);
    void noop();
    str member(const t_asm_val& v, int i, bool is_constant = false);
    str load(const t_asm_val& v);
    void store(const t_asm_val& x, const t_asm_val& y);
    str apply(const str& op, const t_asm_val& x, const t_asm_val& y);
    str convert(const str& op, const t_asm_val& x, const str& t);
    str apply_rel(const str& op, const t_asm_val& x, const t_asm_val& y);
    str apply_rel(const str& op, const t_asm_val& x, const str& y);
    str inc_ptr(const t_asm_val& x, const t_asm_val& y,
                bool is_constant = false);
    str call(const str&, const str&, const vec<t_asm_val>& args);
    void call_void(const str& name, const vec<t_asm_val>& args);
    str bit_not(const t_asm_val& x);
    str phi(const t_asm_val& x, const str& l0,
            const t_asm_val& y, const str& l1);
    void ret(const t_asm_val&);
    void ret();
    void silence(bool);
    bool silence();
    void switch_(const t_asm_val&, const str&, const vec<t_asm_case>&);
    void func_name(const str&);
    void func_return_type(const str&);
    str func_param(const str&);
    void end_func();
    void func_internal(bool);
    void declare(const str& ret_type, const str& name, vec<str> params);
    void declare_external(const str& name, const str& type);
    str def_static_val(const str&);
    str def_global(const str& name, const str& val, bool _internal);
};
