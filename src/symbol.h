#pragma once

#include <common/standard.h>
#include <common/timer.h>

#include <parse/parse.h>
#include <parse/default/block_comment.h>
#include <parse/default/line_comment.h>

#include <parse_ucs/source.h>
#include <parse_cog/factory.h>
#include <parse_chp/factory.h>
#include <parse_prs/factory.h>
#include <parse_spice/factory.h>

#include <chp/graph.h>
#include <interpret_chp/import.h>

#include <hse/graph.h>
#include <interpret_hse/import.h>

#include <prs/production_rule.h>
#include <interpret_prs/import.h>

#include <sch/Netlist.h>
#include <interpret_sch/import.h>

#include <phy/Library.h>
#include <interpret_phy/import.h>

struct Function {
	Function() {}
	~Function() {}

	ucs::variable_set v;
	shared_ptr<chp::graph> cg;
	shared_ptr<hse::graph> hg;
};

struct Structure {
	Structure() {}
	~Structure() {}

	ucs::variable_set v;
	shared_ptr<prs::production_rule_set> pr;
	shared_ptr<sch::Subckt> sp;
};

/*
struct Data {
	Data() {}
	~Data() {}

	...?
};
*/

struct SymbolTable {
	map<string, Function> funcs;
	map<string, Structure> structs;
	// map<string, Data> data;

	map<string, Function>::iterator createFunc(string name);
	map<string, Structure>::iterator createStruct(string name);
	bool load(configuration &config, string path, const Tech *tech=nullptr);
};

