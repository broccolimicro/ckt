#pragma once

#include <weaver/project.h>

void readAstg(weaver::Project &proj, weaver::Source &source, string buffer);
void loadAstg(weaver::Project &proj, weaver::Program &prgm, const weaver::Source &source);
void loadAstgw(weaver::Project &proj, weaver::Program &prgm, const weaver::Source &source);
void writeAstg(fs::path path, weaver::Project &proj, const weaver::Filetype &lang, const weaver::Program &prgm, int modIdx, int termIdx, int varIdx);
void writeAstgw(fs::path path, weaver::Project &proj, const weaver::Filetype &lang, const weaver::Program &prgm, int modIdx, int termIdx, int varIdx);

