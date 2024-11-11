#pragma once

#include <common/standard.h>
#include <parse/parse.h>

void show_help();
int show_command(configuration &config, string techPath, string cellsDir, int argc, char **argv);
