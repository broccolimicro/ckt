#pragma once

#include <common/standard.h>
#include <parse/parse.h>

void tech_help();
int tech_command(configuration &config, string techDir, string techPath, string cellsDir, int argc, char **argv, bool progress=false, bool debug=false);
