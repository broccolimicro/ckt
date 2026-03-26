#pragma once

#include <weaver/project.h>

void readGc(weaver::Project &proj, weaver::Source &source, string buffer);
void loadGc(weaver::Project &proj, weaver::Program &prgm, const weaver::Source &source);
void writeGc(fs::path path, weaver::Project &proj, const weaver::Program &prgm, int modIdx, int termIdx, int varIdx);
std::any factoryGc(string name, const parse::syntax *syntax, tokenizer *tokens);

