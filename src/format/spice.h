#pragma once

#include <choy/project.h>
#include <sch/Subckt.h>

choy::Decl declFromSubckt(const choy::Program &prgm, const sch::Subckt &ckt, std::string name="");
choy::Prototype protoFromInstance(const choy::Program &prgm, const sch::Instance &inst, bool qualify=false);

void readSpice(choy::Project &proj, choy::Source &source, string buffer);
std::vector<choy::TermId> loadSpice(choy::Project &proj, choy::Program &prgm, const choy::Source &source);
void writeSpice(fs::path path, choy::Project &proj, const choy::Filetype &lang, const choy::Program &prgm, choy::TermId id);
std::vector<choy::Prototype> linkSpice(const choy::Project &proj, const choy::Program &prgm, choy::TermId id);
