#pragma once

#include <string>
#include <vector>

struct t_asm_val {
    std::string type;
    std::string name;

    std::string join() const {
        return type + " " + name;
    }
};

struct t_asm_case {
    t_asm_val val;
    std::string label;
};

class t_prog {
    bool _silence = false;
    int str_cnt = 0;
    int label_cnt = 0;
    int id_cnt = 0;
    std::string asm_funcs;
    std::string global_storage;
    std::string asm_type_defs;
    std::string func_body;
    std::string func_var_alloc;

    void a(const std::string&);
    std::string aa(const std::string&);
    void append(std::string&, const std::string&);

public:
    std::string def_str(const std::string& str);
    std::string make_new_id();
    void def_main();
    void def_struct(const std::string& name, const std::string& type);
    std::string def_var(const std::string& type);
    std::string assemble();
    void put_label(const std::string&, bool = true);
    std::string make_label();
    void cond_br(const std::string&, const std::string&, const std::string&);
    void br(const std::string&);
    void noop();
    std::string member(const t_asm_val& v, int i);
    std::string load(const t_asm_val& v);
    void store(const t_asm_val& x, const t_asm_val& y);
    std::string apply(const std::string& op, const t_asm_val& x,
                      const t_asm_val& y);
    std::string convert(const std::string& op, const t_asm_val& x,
                        const std::string& t);
    std::string apply_rel(const std::string& op, const t_asm_val& x,
                          const t_asm_val& y);
    std::string apply_rel(const std::string& op, const t_asm_val& x,
                          const std::string& y);
    std::string inc_ptr(const t_asm_val& x, const t_asm_val& y);
    std::string call_printf(const std::vector<t_asm_val>& args);
    std::string bit_not(const t_asm_val& x);
    std::string phi(const t_asm_val& x, const std::string& l0,
                    const t_asm_val& y, const std::string& l1);
    void ret(const t_asm_val&);
    void silence(bool);
    bool silence();
    void switch_(const t_asm_val&, const std::string&,
                 const std::vector<t_asm_case>&);
};
