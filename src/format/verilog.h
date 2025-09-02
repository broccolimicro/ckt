#pragma once

#include "../weaver/project.h"

void writeVerilog(fs::path path, const weaver::Project &proj, const weaver::Program &prgm, int modIdx, int termIdx);
