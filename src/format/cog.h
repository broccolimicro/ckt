#pragma once

#include <choy/project.h>

void readCog(choy::Project &proj, choy::Source &source, string buffer);
std::vector<choy::TermId> loadCog(choy::Project &proj, choy::Program &prgm, const choy::Source &source);
std::vector<choy::TermId> loadCogw(choy::Project &proj, choy::Program &prgm, const choy::Source &source);
std::any factoryCog(string name, const parse::syntax *syntax, tokenizer *tokens);
std::any factoryCogw(string name, const parse::syntax *syntax, tokenizer *tokens);

