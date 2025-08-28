#pragma once

#include "../weaver/project.h"

void readGds(weaver::Project &proj, weaver::Program &prgm, string path, string buffer);
void writeGds(fs::path path, const weaver::Project &proj, const weaver::Program &prgm, int modIdx, int termIdx);
