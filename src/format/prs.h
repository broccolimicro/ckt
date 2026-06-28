#pragma once

#include <weaver/project.h>

void readPrs(weaver::Project &proj, weaver::Source &source, string buffer);
void loadPrs(weaver::Project &proj, weaver::Program &prgm, const weaver::Source &source);
void writePrs(fs::path path, weaver::Project &proj, const weaver::Filetype &lang, const weaver::Program &prgm, weaver::TermId id);
std::any factoryPrs(string name, const parse::syntax *syntax, tokenizer *tokens);

