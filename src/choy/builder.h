#pragma once

#include <common/standard.h>

#include <choy/program.h>
#include <choy/project.h>

struct Build {
	Build(choy::Project &proj);
	~Build();

	enum {
		LOGIC_RAW = 0,
		LOGIC_CMOS = 1,
		LOGIC_ADIABATIC = 2,
	};

	enum {
		TIMING_MIXED = 0,
		TIMING_QDI = 1,
		TIMING_CLOCKED = 2,
	};

	enum {
		ELAB = 0,
		CONFLICTS = 1,
		ENCODE = 2,
		RULES = 3,
		BUBBLE = 4,
		KEEPERS = 5,
		SIZE = 6,
		NETS = 7,
		MAP = 8,
		CELLS = 9,
		PLACE = 10,
		ROUTE = 11
	};

	choy::Project &proj;

	int logic;
	int timing;
	int stage;

	bool doPreprocess;
	bool doPostprocess;

	bool noCells;
	bool noGhosts;

	bool testDecompose;

	bool progress;
	bool debug;
	bool format_expressions_as_html_table;
	
	vector<bool> targets;

	vector<choy::TermId> todo;

	void set(int target);
	bool get(int target) const;

	void inclAll();
	void incl(int target);
	void excl(int target);
	bool has(int target) const;

	void push(choy::Program &prgm, choy::TermId term=choy::TermId());
	void build(choy::Program &prgm);
};

