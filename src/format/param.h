#pragma once

#include <weaver/program.h>
#include <weaver/instance.h>
#include <map>
#include <string>

weaver::Decl declFromParam(const weaver::Program &prgm, const std::map<std::string, std::string> &params, std::string prefix, int modIdx);
void declToParam(std::map<std::string, std::string> &params, const weaver::Program &prgm, const weaver::Decl &decl, std::string prefix="");
std::map<std::string, std::string> declToParams(const weaver::Program &prgm, const weaver::Decl &decl, std::string prefix="");

weaver::Prototype protoFromParam(const std::map<std::string, std::string> &params, std::string prefix="");
void protoToParam(std::map<std::string, std::string> &params, const weaver::Prototype &proto, std::string prefix="");
std::map<std::string, std::string> protoToParams(const weaver::Prototype &proto, std::string prefix="");
