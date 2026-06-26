#pragma once

#include <weaver/project.h>
#include <sch/Subckt.h>

weaver::Decl declFromSubckt(const weaver::Program &prgm, int mod, const sch::Subckt &ckt);
weaver::Prototype protoFromInstance(const weaver::Program &prgm, int mod, const sch::Instance &inst, bool qualify=false);

void readSpice(weaver::Project &proj, weaver::Source &source, string buffer);
void loadSpice(weaver::Project &proj, weaver::Program &prgm, const weaver::Source &source);
void writeSpice(fs::path path, weaver::Project &proj, const weaver::Filetype &lang, const weaver::Program &prgm, int modIdx, int termIdx, int varIdx);

