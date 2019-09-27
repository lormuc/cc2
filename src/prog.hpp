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
    int str_cnt = 0;
    int label_cnt = 0;
    int id_cnt = 0;
    str asm_funcs;
    str global_storage;
    str asm_type_defs;
    str func_body;
    str func_var_alloc;

    void a(const str&);
    str aa(const str&);
    void append(str&, const str&);

public:
    str def_str(const str& str);
    str make_new_id();
    void def_main();
    void def_struct(const str& name, const str& type);
    str def_var(const str& type);
    str assemble();
    void put_label(const str&, bool = true);
    str make_label();
    void cond_br(const str&, const str&, const str&);
    void br(const str&);
    void noop();
    str member(const t_asm_val& v, int i);
    str load(const t_asm_val& v);
    void store(const t_asm_val& x, const t_asm_val& y);
    str apply(const str& op, const t_asm_val& x, const t_asm_val& y);
    str convert(const str& op, const t_asm_val& x, const str& t);
    str apply_rel(const str& op, const t_asm_val& x, const t_asm_val& y);
    str apply_rel(const str& op, const t_asm_val& x, const str& y);
    str inc_ptr(const t_asm_val& x, const t_asm_val& y);
    str call_printf(const vec<t_asm_val>& args);
    str bit_not(const t_asm_val& x);
    str phi(const t_asm_val& x, const str& l0,
            const t_asm_val& y, const str& l1);
    void ret(const t_asm_val&);
    void silence(bool);
    bool silence();
    void switch_(const t_asm_val&, const str&, const vec<t_asm_case>&);
};
