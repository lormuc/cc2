#include <string>

#include "prog.hpp"
#include "misc.hpp"

namespace {
    str func_line(const str& x) {
        return str("    ") + x + "\n";
    }

    str deref(const str& type) {
        return type.substr(0, type.length() - 1);
    }
}

void t_prog::append(str& x, const str& y) {
    if (not silence()) {
        x += y;
    }
}

str t_prog::make_new_id() {
    id_cnt++;
    return "%_" + std::to_string(id_cnt);
}

str t_prog::make_new_global_id() {
    id_cnt++;
    return "@_" + std::to_string(id_cnt);
}

void t_prog::a(const str& line) {
    append(func_body, func_line(line));
}

str t_prog::aa(const str& line) {
    _ res = make_new_id();
    a(res + " = " + line);
    return res;
}

void t_prog::cond_br(const str& v, const str& a1, const str& a2) {
    a("br i1 " + v + ", label " + a1 + ", label " + a2);
}

void t_prog::br(const str& l) {
    a("br label " + l);
}

str t_prog::make_label() {
    label_cnt++;
    return "%l_" + std::to_string(label_cnt);
}

void t_prog::put_label(const str& l, bool f) {
    if (f) {
        a("br label " + l);
    }
    append(func_body, l.substr(1) + ":\n");
}

str t_prog::def_str(const str& str) {
    _ len = str.length() + 1;
    _ name = make_new_global_id();
    append(global_storage, name);
    append(global_storage, " = private unnamed_addr constant [");
    append(global_storage, std::to_string(len));
    append(global_storage, " x i8] c\"");
    append(global_storage, print_bytes(str));
    append(global_storage, "\\00\"\n");
    return name;
}

void t_prog::def_struct(const str& name, const str& type) {
    append(asm_type_defs, name + " = type " + type + "\n");
}

void t_prog::def_opaque_struct(const str& name) {
    append(asm_type_defs, name + " = type opaque\n");
}

str t_prog::assemble() {
    str res;
    res += asm_type_defs;
    res += "\n";
    res += global_storage;
    res += "\n";
    res += asm_funcs;
    res += "\n";
    res += "declare i32 @printf(i8*, ...)\n";
    res += "declare i32 @snprintf(i8*, i64, i8*, ...)\n";
    return res;
}

void t_prog::noop() {
    aa("add i1 0, 0");
}

str t_prog::def_var(const str& type) {
    _ res = make_new_id();
    append(func_var_alloc, func_line(res + " = alloca " + type));
    return res;
}

str t_prog::def_static(const str& type) {
    _ res = make_new_global_id();
    append(global_storage, res + " = internal global "
           + type + " zeroinitializer\n");
    return res;
}

str t_prog::member(const t_asm_val& v, int i) {
    return aa("getelementptr inbounds " + deref(v.type) + ", " + v.join()
              + ", i32 0, i32 " + std::to_string(i));
}

str t_prog::load(const t_asm_val& v) {
    return aa("load " + deref(v.type) + ", " + v.join());
}

void t_prog::store(const t_asm_val& x, const t_asm_val& y) {
    a("store " + x.join() + ", " + y.join());
}

str t_prog::apply(const str& op, const t_asm_val& x,
                  const t_asm_val& y) {
    return aa(op + " " + x.type + " " + x.name + ", " + y.name);
}

str t_prog::apply_rel(const str& op, const t_asm_val& x,
                      const t_asm_val& y) {
    _ tmp = aa(op + " " + x.join() + ", " + y.name);
    return convert("zext", {"i1", tmp}, "i32");
}

str t_prog::apply_rel(const str& op, const t_asm_val& x,
                      const str& y) {
    _ tmp = aa(op + " " + x.join() + ", " + y);
    return convert("zext", {"i1", tmp}, "i32");
}

str t_prog::convert(const str& op, const t_asm_val& x,
                    const str& t) {
    return aa(op + " " + x.join() + " to " + t);
}

str t_prog::inc_ptr(const t_asm_val& x, const t_asm_val& y) {
    return aa("getelementptr inbounds " + deref(x.type) + ", " + x.join()
              + ", " + y.join());
}

str t_prog::call(const str& ret_type, const str& name,
                 const vec<t_asm_val>& args) {
    str args_str;
    for (_& arg : args) {
        if (args_str.empty()) {
            args_str += arg.join();
        } else {
            args_str += ", " + arg.join();
        }
    }
    return aa("call " + ret_type + " " + name + "(" + args_str + ")");
}

void t_prog::call_void(const str& name, const vec<t_asm_val>& args) {
    str args_str;
    for (_& arg : args) {
        if (args_str.empty()) {
            args_str += arg.join();
        } else {
            args_str += ", " + arg.join();
        }
    }
    a("call void " + name + "(" + args_str + ")");
}

str t_prog::bit_not(const t_asm_val& x) {
    return aa("xor " + x.join() + ", -1");
}

str t_prog::phi(const t_asm_val& x, const str& l0,
                const t_asm_val& y, const str& l1) {
    return aa("phi " + x.type + " [ " + x.name + ", " + l0 + " ], [ "
              + y.name + ", " + l1 + " ]");
}

void t_prog::ret(const t_asm_val& x) {
    a("ret " + x.join());
}

void t_prog::ret() {
    a("ret void");
}

void t_prog::silence(bool x) {
    _silence = x;
}

bool t_prog::silence() {
    return _silence;
}

void t_prog::switch_(const t_asm_val& x, const str& default_label,
                     const vec<t_asm_case>& cases) {
    str str;
    for (_& c : cases) {
        if (str != "") {
            str += " ";
        }
        str += c.val.join() + ", label " + c.label;
    }
    a("switch " + x.join() + ", label " + default_label
      + " [" + str + "]");
}

void t_prog::func_name(const str& x) {
    _func_name = x;
}

void t_prog::func_return_type(const str& x) {
    _func_return_type = x;
}

str t_prog::func_param(const str& t) {
    _ as = def_var(t);
    _ param_idx = "%" + std::to_string(func_params.size());
    store({t, param_idx}, {t + "*", as});
    func_params.push_back(t);
    return as;
}

void t_prog::end_func() {
    append(asm_funcs, "define " + _func_return_type + " " + _func_name);
    str params;
    for (_& p : func_params) {
        if (params != "") {
            params += ", ";
        }
        params += p;
    }
    append(asm_funcs, "(" + params + ") {\n");
    append(asm_funcs, func_var_alloc);
    append(asm_funcs, func_body);
    append(asm_funcs, "}\n\n");
    func_params.clear();
    func_body = "";
    func_var_alloc = "";
}
