#pragma once

#include <string>

#include "asm_gen.hpp"
#include "ast.hpp"

t_val gen_convert_assign(const t_val& lhs, const t_val& rhs, t_ctx& ctx);
t_val gen_assign(const t_val& lhs, const t_val& rhs, t_ctx& ctx);
t_val gen_array_elt(const t_val& v, int i, t_ctx& ctx);
t_val gen_struct_member(const t_val& v, int i, t_ctx& ctx);
t_val gen_is_zero(const t_val& x, const t_ctx& ctx);
std::string gen_is_zero_i1(const t_val& x, const t_ctx& ctx);
t_val gen_is_nonzero(const t_val& x, const t_ctx& ctx);
std::string gen_is_nonzero_i1(const t_val& x, const t_ctx& ctx);
t_val gen_convert_assign(const t_val& lhs, const t_val& rhs, t_ctx& ctx);
t_val gen_exp(const t_ast& ast, t_ctx& ctx, bool convert_lvalue = true);
