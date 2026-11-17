#pragma once

#include <choy/project.h>

void readAstg(choy::Project &proj, choy::Source &source, string buffer);
std::vector<choy::TermId> loadAstg(choy::Project &proj, choy::Program &prgm, const choy::Source &source);
std::vector<choy::TermId> loadAstgw(choy::Project &proj, choy::Program &prgm, const choy::Source &source);
void writeAstg(fs::path path, choy::Project &proj, const choy::Filetype &lang, const choy::Program &prgm, choy::TermId id);
void writeAstgw(fs::path path, choy::Project &proj, const choy::Filetype &lang, const choy::Program &prgm, choy::TermId id);

