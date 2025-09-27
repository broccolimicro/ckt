#pragma once

#include <common/net.h>

#include <parse/tokenizer.h>
#include <parse_ucs/source.h>
#include <parse_ucs/expression.h>

#include <vector>
#include <string>

#include <weaver/program.h>

#include <phy/Tech.h>

using std::vector;
using std::string;

namespace weaver {

// Managing scope
/*bool define(vector<string> typeName, string name, vector<int> size, ucs::Netlist nets);
void pushScope();
void popScope();*/

// Loading the program
bool import_declaration(vector<Instance> &result, const Program &prgm, int modIdx, const parse_ucs::function::declaration &syntax, tokenizer *tokens);
Decl import_prototype(const Program &prgm, int modIdx, const parse_ucs::prototype &syntax, TypeId recvType, tokenizer *tokens);
void import_symbols(Program &prgm, int modIdx, const parse_ucs::source &syntax, tokenizer *tokens);
void import_module(Program &prgm, int modIdx, const parse_ucs::source &syntax, tokenizer *tokens);

}
