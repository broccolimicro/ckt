#pragma once

#include <common/standard.h>
#include <parse/parse.h>

void compare_help();
int compare_command(configuration &config, string techPath, string cellsDir, int argc, char **argv, bool progress=false, bool debug=false);
