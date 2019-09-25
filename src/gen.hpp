#pragma once

#include <string>
#include <unordered_map>
#include <stdexcept>
#include <cassert>
#include <iostream>
#include <functional>
#include <set>

#include "ast.hpp"
#include "type.hpp"
#include "prog.hpp"
#include "ctx.hpp"

std::string make_new_id();
std::string make_label();
void put_label(const std::string& l, bool f = true);
std::string func_line(const std::string&);
t_type make_base_type(const t_ast& t, t_ctx& ctx);
std::string unpack_declarator(t_type& type, const t_ast& t, t_ctx& ctx);
std::string gen_asm(const t_ast&);

extern t_prog prog;
