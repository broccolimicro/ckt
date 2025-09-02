#pragma once

#include <common/standard.h>
#include <common/net.h>
#include <boolean/cube.h>

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

	void create(string prefix, ucs::ConstNetlist nets);
	void append(uint64_t t, boolean::cube encoding, string error="");
	void append(uint64_t t, boolean::cube encoding, boolean::cube strength, string error="");
	void close();
};

