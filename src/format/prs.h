#pragma once

#include <choy/project.h>

void readPrs(choy::Project &proj, choy::Source &source, string buffer);
std::vector<choy::TermId> loadPrs(choy::Project &proj, choy::Program &prgm, const choy::Source &source);
void writePrs(fs::path path, choy::Project &proj, const choy::Filetype &lang, const choy::Program &prgm, choy::TermId id);
std::any factoryPrs(string name, const parse::syntax *syntax, tokenizer *tokens);

