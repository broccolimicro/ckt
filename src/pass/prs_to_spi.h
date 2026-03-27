#pragma once

#include "../weaver/builder.h"
#include <weaver/program.h>
#include <weaver/term.h>

bool bubble(const Build &builder, weaver::Program &prgm, weaver::TermId id);
bool keepers(const Build &builder, weaver::Program &prgm, weaver::TermId id);
bool sizing(const Build &builder, weaver::Program &prgm, weaver::TermId id);
bool prsToSpi(Build &builder, weaver::Program &prgm, weaver::TermId id);

