#pragma once

#include <common/standard.h>
#include <parse/parse.h>

int build_command(configuration &config, int argc, char **argv, bool progress, bool debug);
