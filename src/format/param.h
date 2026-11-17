#pragma once

#include <choy/program.h>
#include <choy/instance.h>
#include <map>
#include <string>

choy::Decl declFromParam(const choy::Program &prgm, const std::map<std::string, std::string> &params, std::string prefix, int modIdx);
void declToParam(std::map<std::string, std::string> &params, const choy::Program &prgm, const choy::Decl &decl, std::string prefix="");
std::map<std::string, std::string> declToParams(const choy::Program &prgm, const choy::Decl &decl, std::string prefix="");

choy::Prototype protoFromParam(const std::map<std::string, std::string> &params, std::string prefix="");
void protoToParam(std::map<std::string, std::string> &params, const choy::Prototype &proto, std::string prefix="");
std::map<std::string, std::string> protoToParams(const choy::Prototype &proto, std::string prefix="");
