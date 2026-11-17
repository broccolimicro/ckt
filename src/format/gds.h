#pragma once

#include <choy/project.h>

std::vector<choy::TermId> loadGds(choy::Project &proj, choy::Program &prgm, const choy::Source &source);
void writeGds(fs::path path, choy::Project &proj, const choy::Filetype &lang, const choy::Program &prgm, choy::TermId id);

