#pragma once

#include <choy/project.h>

void readWv(choy::Project &proj, choy::Source &source, string buffer);
std::vector<choy::TermId> loadWv(choy::Project &proj, choy::Program &prgm, const choy::Source &source);
