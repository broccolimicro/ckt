#pragma once

#include "../weaver/builder.h"
#include <weaver/program.h>
#include <weaver/term.h>

bool flatten(const Build &builder, weaver::Program &prgm, weaver::TermId id);
bool decompose(Build &builder, weaver::Program &prgm, weaver::TermId id);
bool chpToFlow(Build &builder, weaver::Program &prgm, weaver::TermId id);

