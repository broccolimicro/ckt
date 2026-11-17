#pragma once

#include "../choy/builder.h"
#include <choy/program.h>
#include <choy/term.h>

bool flatten(const Build &builder, choy::Program &prgm, choy::TermId id);
bool decompose(Build &builder, choy::Program &prgm, choy::TermId id);
bool chpToFlow(Build &builder, choy::Program &prgm, choy::TermId id);

