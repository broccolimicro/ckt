#pragma once

#include <common/standard.h>
#include <parse/parse.h>

void build_help();
int build_command(configuration &config, int argc, char **argv, bool progress, bool debug);
