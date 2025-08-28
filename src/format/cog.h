#pragma once

#include "../weaver/project.h"

void readCog(weaver::Project &proj, weaver::Source &source, string buffer);
void loadCog(weaver::Project &proj, weaver::Program &prgm, const weaver::Source &source);
void loadCogw(weaver::Project &proj, weaver::Program &prgm, const weaver::Source &source);
