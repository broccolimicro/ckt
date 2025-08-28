#pragma once

#include "../weaver/project.h"

void readCog(weaver::Project &proj, weaver::Source &source, string buffer);
void loadCog(weaver::Project &proj, weaver::Program &prgm, const weaver::Source &source);
void loadCogw(weaver::Project &proj, weaver::Program &prgm, const weaver::Source &source);
std::any factoryCog(const parse::syntax *syntax, tokenizer *tokens);
std::any factoryCogw(const parse::syntax *syntax, tokenizer *tokens);

