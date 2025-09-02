#pragma once

#include "../weaver/project.h"

void readSpice(weaver::Project &proj, weaver::Source &source, string buffer);
void loadSpice(weaver::Project &proj, weaver::Program &prgm, const weaver::Source &source);
void writeSpice(fs::path path, const weaver::Project &proj, const weaver::Program &prgm, int modIdx, int termIdx);
