#pragma once

#include <string>

#include "gen.hpp"
#include "ast.hpp"

t_val gen_convert_assign(const t_val& lhs, const t_val& rhs, t_ctx& ctx);
t_val gen_assign(const t_val& lhs, const t_val& rhs, t_ctx& ctx);
t_val gen_array_elt(const t_val& v, size_t i, t_ctx& ctx);
t_val gen_is_zero(const t_val& x, const t_ctx& ctx);
str gen_is_zero_i1(const t_val& x, const t_ctx& ctx);
t_val gen_is_nonzero(const t_val& x, const t_ctx& ctx);
str gen_is_nonzero_i1(const t_val& x, const t_ctx& ctx);
t_val gen_convert_assign(const t_val& lhs, const t_val& rhs, t_ctx& ctx);
t_val gen_exp(const t_ast& ast, t_ctx& ctx);
void gen_int_promotion(t_val& x, const t_ctx& ctx);
t_val gen_conversion(const t_type& t, const t_val& v, const t_ctx& ctx);
