#pragma once

#include <weaver/project.h>

void readGc(weaver::Project &proj, weaver::Source &source, string buffer);
std::vector<weaver::TermId> loadGc(weaver::Project &proj, weaver::Program &prgm, const weaver::Source &source);
void writeGc(fs::path path, weaver::Project &proj, const weaver::Filetype &lang, const weaver::Program &prgm, weaver::TermId id);
std::any factoryGc(string name, const parse::syntax *syntax, tokenizer *tokens);

