#pragma once

#include "../weaver/project.h"

void loadGds(weaver::Project &proj, weaver::Program &prgm, const weaver::Source &source);
void writeGds(fs::path path, const weaver::Project &proj, const weaver::Program &prgm, int modIdx, int termIdx);
