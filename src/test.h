#pragma once

#include <common/standard.h>
#include <parse/parse.h>

void test_help();
int test_command(configuration &config, string techPath, string cellsDir, int argc, char **argv);
