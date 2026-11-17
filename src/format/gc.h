#pragma once

#include <choy/project.h>

void readGc(choy::Project &proj, choy::Source &source, string buffer);
std::vector<choy::TermId> loadGc(choy::Project &proj, choy::Program &prgm, const choy::Source &source);
void writeGc(fs::path path, choy::Project &proj, const choy::Filetype &lang, const choy::Program &prgm, choy::TermId id);
std::any factoryGc(string name, const parse::syntax *syntax, tokenizer *tokens);

