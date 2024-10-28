#pragma once

#include <common/standard.h>
#include <parse/parse.h>

void compare_help();
int compare_command(configuration &config, int argc, char **argv, bool progress=false, bool debug=false);
