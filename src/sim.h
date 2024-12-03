#pragma once

#include <common/standard.h>
#include <parse/parse.h>

void sim_help();
int sim_command(configuration &config, string techPath, string cellsDir, int argc, char **argv, bool debug);
