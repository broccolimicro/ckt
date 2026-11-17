#pragma once

#include <choy/project.h>

void writeVerilog(fs::path path, choy::Project &proj, const choy::Filetype &lang, const choy::Program &prgm, choy::TermId id);

