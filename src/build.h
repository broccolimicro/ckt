#pragma once

#include <common/standard.h>
#include <parse/parse.h>

void build_help();
int build_command(string workingDir, string techPath, string cellsDir, int argc, char **argv, bool progress, bool debug);
