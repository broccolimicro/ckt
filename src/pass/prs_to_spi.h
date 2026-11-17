#pragma once

#include "../choy/builder.h"
#include <choy/program.h>
#include <choy/term.h>

bool bubble(const Build &builder, choy::Program &prgm, choy::TermId id);
bool keepers(const Build &builder, choy::Program &prgm, choy::TermId id);
bool sizing(const Build &builder, choy::Program &prgm, choy::TermId id);
bool prsToSpi(Build &builder, choy::Program &prgm, choy::TermId id);

