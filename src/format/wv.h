#pragma once

#include <weaver/project.h>

void readWv(weaver::Project &proj, weaver::Source &source, string buffer);
std::vector<weaver::TermId> loadWv(weaver::Project &proj, weaver::Program &prgm, const weaver::Source &source);
