#pragma once

#include <weaver/project.h>

std::vector<weaver::TermId> loadGds(weaver::Project &proj, weaver::Program &prgm, const weaver::Source &source);
void writeGds(fs::path path, weaver::Project &proj, const weaver::Filetype &lang, const weaver::Program &prgm, weaver::TermId id);

