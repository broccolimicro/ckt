#pragma once

#include "../weaver/builder.h"
#include <weaver/program.h>
#include <weaver/term.h>

bool elaborate(const Build &builder, weaver::Program &prgm, weaver::TermId id);
bool conflicts(const Build &builder, weaver::Program &prgm, weaver::TermId id);
bool encode(const Build &builder, weaver::Program &prgm, weaver::TermId id);
bool hseToPrs(Build &builder, weaver::Program &prgm, weaver::TermId id);
