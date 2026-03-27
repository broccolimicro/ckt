#pragma once

#include "../weaver/builder.h"
#include <weaver/program.h>
#include <weaver/term.h>

bool flatten(Build &builder, weaver::Program &prgm, weaver::TermId id);
bool decompose(Build &builder, weaver::Program &prgm, weaver::TermId id, vector<weaver::TermId> &dst);
bool chpToFlow(Build &builder, weaver::Program &prgm, weaver::TermId id, vector<weaver::TermId> &dst);

