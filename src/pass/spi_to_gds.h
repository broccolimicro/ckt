#pragma once

#include "../choy/builder.h"
#include <choy/program.h>
#include <choy/term.h>

bool mapCells(Build &builder, choy::Program &prgm, choy::TermId id);
bool spiToGds(Build &builder, choy::Program &prgm, choy::TermId id);
//bool placeCells(const Build &builder, choy::Program &prgm, choy::TermId id);

