#pragma once

#include <common/standard.h>
#include <parse/parse.h>

void unpack_help();
int unpack_command(configuration &config, string techPath, string cellsDir, int argc, char **argv, bool progress, bool debug);
