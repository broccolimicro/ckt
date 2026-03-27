#pragma once

#include "../weaver/builder.h"
#include <weaver/program.h>
#include <weaver/term.h>

bool flowToVerilog(Build &builder, weaver::Program &prgm, weaver::TermId id, std::vector<weaver::TermId> &dst);

