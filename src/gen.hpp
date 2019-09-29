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

str make_new_id();
str make_label();
void put_label(const str& l, bool f = true);
str func_line(const str&);
t_type make_base_type(const t_ast& t, t_ctx& ctx);
str unpack_declarator(t_type& type, const t_ast& t, t_ctx& ctx, bool = false);
str gen_asm(const t_ast&);

extern t_prog prog;
