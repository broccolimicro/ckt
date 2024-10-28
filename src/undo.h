#pragma once

#include <common/standard.h>
#include <parse/parse.h>

void undo_help();
int undo_command(configuration &config, int argc, char **argv, bool progress, bool debug);
