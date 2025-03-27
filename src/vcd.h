#pragma once

#include <common/standard.h>
#include <boolean/cube.h>
#include <ucs/variable.h>
#include <interpret_boolean/interface.h>

struct vcd {
	vcd();
	~vcd();

	FILE *fvcd;
	FILE *fgtk;
	vector<string> nets;
	vector<pair<uint64_t, string> > markers;
	uint64_t t;

	boolean::cube curr;

	string &at(int net);

	void create(string prefix, const ucs::variable_set &v);
	void create(string prefix, boolean::ConstNetlist nets);
	void append(uint64_t t, boolean::cube encoding, string error="");
	void append(uint64_t t, boolean::cube encoding, boolean::cube strength, string error="");
	void close();
};

