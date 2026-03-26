#pragma once

#include "../weaver/builder.h"
#include <weaver/program.h>
#include <weaver/term.h>

bool flatten(Build &builder, weaver::Program &prgm, weaver::TermId id, int index);
bool decompose(Build &builder, weaver::Program &prgm, weaver::TermId id, int index);
bool bind(Build &builder, weaver::Program &prgm, weaver::TermId id, int index);
bool chpToFlow(Build &builder, weaver::Program &prgm, weaver::TermId id, int index);

