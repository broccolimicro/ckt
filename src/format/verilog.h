#pragma once

#include <weaver/project.h>

void writeVerilog(fs::path path, weaver::Project &proj, const weaver::Filetype &lang, const weaver::Program &prgm, int modIdx, int termIdx, int varIdx);

