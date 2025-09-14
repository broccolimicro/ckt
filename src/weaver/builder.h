#pragma once

#include <common/standard.h>
#include <parse/parse.h>

#include <weaver/program.h>
#include <phy/Tech.h>

#include "project.h"

struct Build {
	Build(weaver::Project &proj);
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

	weaver::Project &proj;

	int logic;
	int timing;
	int stage;

	bool doPreprocess;
	bool doPostprocess;

	bool noCells;
	bool noGhosts;

	bool progress;
	bool debug;
	bool format_expressions_as_html_table;
	
	vector<bool> targets;

	void set(int target);
	bool get(int target) const;

	void inclAll();
	void incl(int target);
	void excl(int target);
	bool has(int target) const;

	void build(weaver::Program &prgm);

	// TODO(edward.bingham) generalize this into lowering and analysis stages, create a DAG to generalize the compilation algorithm
	bool chpToFlow(weaver::Program &prgm, int modIdx, int termIdx) const;
	bool flowToVerilog(weaver::Program &prgm, int modIdx, int termIdx) const;

	bool hseToPrs(weaver::Program &prgm, int modIdx, int termIdx) const;
	bool prsToSpi(weaver::Program &prgm, int modIdx, int termIdx);
	bool spiToGds(weaver::Program &prgm, int modIdx, int termIdx);
};

