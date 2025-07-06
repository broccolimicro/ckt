#pragma once

#include <common/standard.h>
#include <parse/parse.h>

struct Build {
	Build() {
		logic = LOGIC_CMOS;
		stage = -1;

		doPreprocess = false;
		doPostprocess = false;

		noCells = false;
		noGhosts = false;
		
		targets.resize(ROUTE+1, false);
	}

	~Build() {
	}

	enum {
		LOGIC_RAW = 0,
		LOGIC_CMOS = 1,
		LOGIC_ADIABATIC = 2
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

	int logic;
	int stage;

	bool doPreprocess;
	bool doPostprocess;

	bool noCells;
	bool noGhosts;

	bool progress;
	bool debug;
	
	vector<bool> targets;

	void set(int target);
	bool get(int target);

	void inclAll();
	void incl(int target);
	void excl(int target);
	bool has(int target);
};

