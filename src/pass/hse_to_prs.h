#pragma once

#include "../choy/builder.h"
#include <choy/program.h>
#include <choy/term.h>

bool elaborate(const Build &builder, choy::Program &prgm, choy::TermId id);
bool conflicts(const Build &builder, choy::Program &prgm, choy::TermId id);
bool encode(const Build &builder, choy::Program &prgm, choy::TermId id);
bool hseToPrs(Build &builder, choy::Program &prgm, choy::TermId id);
