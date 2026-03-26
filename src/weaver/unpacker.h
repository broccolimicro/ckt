#pragma once

#include <common/standard.h>
#include <parse/parse.h>

#include <weaver/program.h>
#include <weaver/project.h>

struct Unpack {
	Unpack(weaver::Project &proj);
	~Unpack();

	enum {
		LAYOUT = 0,
		NETS = 1,
		SIZED = 2
	};

	weaver::Project &proj;

	int stage;

	bool progress;
	bool debug;
	
	vector<bool> targets;

	void set(int target);
	bool get(int target) const;

	void inclAll();
	void incl(int target);
	void excl(int target);
	bool has(int target) const;
};

