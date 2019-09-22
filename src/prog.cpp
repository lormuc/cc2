#include <string>

#include "prog.hpp"
#include "misc.hpp"

using namespace std;

namespace {
    string func_line(const string& x) {
        return string("    ") + x + "\n";
    }

    string deref(const string& type) {
        return type.substr(0, type.length() - 1);
    }
}

void t_prog::append(std::string& x, const std::string& y) {
    if (not silence()) {
        x += y;
    }
}

string t_prog::make_new_id() {
    id_cnt++;
    return "%_" + to_string(id_cnt);
}

void t_prog::a(const std::string& line) {
    append(func_body, func_line(line));
}

string t_prog::aa(const std::string& line) {
    auto res = make_new_id();
    a(res + " = " + line);
    return res;
}

void t_prog::cond_br(const string& v, const string& a1, const string& a2) {
    a("br i1 " + v + ", label " + a1 + ", label " + a2);
}

void t_prog::br(const string& l) {
    a("br label " + l);
}

string t_prog::make_label() {
    label_cnt++;
    return "%l_" + to_string(label_cnt);
}

void t_prog::put_label(const string& l, bool f) {
    if (f) {
        a("br label " + l);
    }
    append(func_body, l.substr(1) + ":\n");
}

string t_prog::def_str(const string& str) {
    auto len = str.length() + 1;
    auto name = "@str_" + to_string(str_cnt);
    str_cnt++;
    append(global_storage, name);
    append(global_storage, " = private unnamed_addr constant [");
    append(global_storage, to_string(len));
    append(global_storage, " x i8] c\"");
    append(global_storage, print_bytes(str));
    append(global_storage, "\\00\"\n");
    return name;
}

void t_prog::def_main() {
    append(asm_funcs, "define i32 @");
    append(asm_funcs, "main");
    append(asm_funcs, "() {\n");
    append(asm_funcs, func_var_alloc);
    append(asm_funcs, func_body);
    append(asm_funcs, "}\n");
}

void t_prog::def_struct(const string& name, const string& type) {
    append(asm_type_defs, name + " = type " + type + "\n");
}

string t_prog::assemble() {
    string res;
    res += asm_type_defs;
    res += "\n";
    res += global_storage;
    res += "\n";
    res += asm_funcs;
    res += "\n";
    res += "declare i32 @printf(i8* nocapture readonly, ...)\n";
    return res;
}

void t_prog::noop() {
    aa("add i1 0, 0");
}

string t_prog::def_var(const string& type) {
    auto res = make_new_id();
    append(func_var_alloc, func_line(res + " = alloca " + type));
    return res;
}

string t_prog::member(const t_asm_val& v, int i) {
    return aa("getelementptr inbounds " + deref(v.type) + ", " + v.join()
              + ", i32 0, i32 " + to_string(i));
}

string t_prog::load(const t_asm_val& v) {
    return aa("load " + deref(v.type) + ", " + v.join());
}

void t_prog::store(const t_asm_val& x, const t_asm_val& y) {
    a("store " + x.join() + ", " + y.join());
}

string t_prog::apply(const string& op, const t_asm_val& x,
                     const t_asm_val& y) {
    return aa(op + " " + x.type + " " + x.name + ", " + y.name);
}

string t_prog::apply_rel(const string& op, const t_asm_val& x,
                         const t_asm_val& y) {
    auto tmp = aa(op + " " + x.join() + ", " + y.name);
    return convert("zext", {"i1", tmp}, "i32");
}

string t_prog::apply_rel(const string& op, const t_asm_val& x,
                         const string& y) {
    auto tmp = aa(op + " " + x.join() + ", " + y);
    return convert("zext", {"i1", tmp}, "i32");
}

string t_prog::convert(const string& op, const t_asm_val& x,
                       const string& t) {
    return aa(op + " " + x.join() + " to " + t);
}

string t_prog::inc_ptr(const t_asm_val& x, const t_asm_val& y) {
    return aa("getelementptr inbounds " + deref(x.type) + ", " + x.join()
              + ", " + y.join());
}

string t_prog::call_printf(const vector<t_asm_val>& args) {
    string args_str;
    for (auto& arg : args) {
        if (args_str.empty()) {
            args_str += arg.join();
        } else {
            args_str += ", " + arg.join();
        }
    }
    return aa("call i32 (i8*, ...) @printf(" + args_str + ")");
}

string t_prog::bit_not(const t_asm_val& x) {
    return aa("xor " + x.join() + ", -1");
}

string t_prog::phi(const t_asm_val& x, const string& l0,
                   const t_asm_val& y, const string& l1) {
    return aa("phi " + x.type + " [ " + x.name + ", " + l0 + " ], [ "
              + y.name + ", " + l1 + " ]");
}

void t_prog::ret(const t_asm_val& x) {
    a("ret " + x.join());
}

void t_prog::silence(bool x) {
    _silence = x;
}

bool t_prog::silence() {
    return _silence;
}
