#pragma once

#include <weaver/project.h>
#include <sch/Subckt.h>

weaver::Decl declFromSubckt(const weaver::Program &prgm, const sch::Subckt &ckt, std::string name="");
weaver::Prototype protoFromInstance(const weaver::Program &prgm, const sch::Instance &inst, bool qualify=false);

void readSpice(weaver::Project &proj, weaver::Source &source, string buffer);
std::vector<weaver::TermId> loadSpice(weaver::Project &proj, weaver::Program &prgm, const weaver::Source &source);
void writeSpice(fs::path path, weaver::Project &proj, const weaver::Filetype &lang, const weaver::Program &prgm, weaver::TermId id);
std::vector<weaver::Prototype> linkSpice(const weaver::Project &proj, const weaver::Program &prgm, weaver::TermId id);
