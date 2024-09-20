#pragma once

#include <common/standard.h>
#include <parse/parse.h>

void verify_help();
int verify_command(configuration &config, int argc, char **argv);
