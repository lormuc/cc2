#pragma once

#include <list>

#include "lex.hpp"
#include "ast.hpp"
#include "file.hpp"

typedef std::list<t_pp_lexeme> t_pp_seq;
typedef std::list<t_pp_lexeme>::iterator t_pp_iter;
typedef std::list<t_pp_lexeme>::const_iterator t_pp_c_iter;

void preprocess(t_pp_seq&, t_file_manager&);
std::list<t_lexeme> convert_lexemes(t_pp_c_iter it, t_pp_c_iter fin);
void escape_seqs(t_pp_iter it, t_pp_iter fin);
