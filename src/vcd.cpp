#include "vcd.h"

#include <interpret_ucs/export.h>
#include <unistd.h>

vcd::vcd() {
	fvcd = nullptr;
	fgtk = nullptr;
	t = 0;
}

vcd::~vcd() {
	close();
}

string &vcd::at(int net) {
	if (net < 0) {
		return nodes[-net-1];
	}
	return nets[net];
}

void vcd::create(string prefix, const ucs::variable_set &v, int nodes) {
	time_t rawtime;
	tm *timeinfo;
	char buffer[1024];

	time (&rawtime);
	timeinfo = localtime(&rawtime);
	strftime(buffer,sizeof(buffer),"%Y-%m-%d",timeinfo);

	fvcd = fopen((prefix+".vcd").c_str(), "w");
	fprintf(fvcd, "$date\n");
	fprintf(fvcd, "%s\n", buffer);
	fprintf(fvcd, "$end\n");
	fprintf(fvcd, "$timescale 1ps $end\n");

	if (nodes > (int)this->nodes.size()) {
		this->nodes.resize(nodes);
	}
	if ((int)v.nodes.size() > (int)nets.size()) {
		nets.resize(v.nodes.size());
	} 
	string id = {33,33,33};
	for (int i = -nodes; i < (int)v.nodes.size(); i++) {
		string name = export_variable_name(i, v).to_string();
		at(i) = id;
		if (i < 0) {
			curr.set((int)v.nodes.size()-i-1, -1);
		} else {
			curr.set(i, -1);
		}

		for (int j = (int)name.size(); j >= 0; j--) {
			if (string(":.").find(name[j]) != string::npos) {
				name[j] = '_';
			} else if (string("[]").find(name[j]) != string::npos) {
				name.erase(name.begin()+j);
			}
		}
	
		fprintf(fvcd, "$var wire %d %s %s $end\n", 1, id.c_str(), name.c_str());

		id[2] += 1;
		if (id[2] > 126) {
			id[1] += 1;
			id[2] = 33;
		}
		if (id[1] > 126) {
			id[0] += 1;
			id[1] = 33;
		}
	}

	fprintf(fvcd, "$enddefinitions $end\n");

	fprintf(fvcd, "$dumpvars\n");
	for (int i = -nodes; i < (int)nets.size(); i++) {
		//if (value[1] == 1) {
			fprintf(fvcd, "x%s\n", at(i).c_str());
		//} else {
		//	fprintf(fvcd, "b%s %s", "x"*value[1], value[0]);
		//}
	}
	fprintf(fvcd, "$end\n");

	getcwd(buffer, 1024);

	fgtk = fopen((prefix+".gtkw").c_str(), "w");
	fprintf(fgtk, "[dumpfile] \"%s/%s.vcd\"\n", buffer, prefix.c_str());
	fprintf(fgtk, "[savefile] \"%s/%s.gtkw\"\n", buffer, prefix.c_str());
}

void vcd::append(uint64_t t, boolean::cube encoding, string error) {
	static const char values[4] = {'x','0','1','z'};
	if (t > this->t) {
		fprintf(fvcd, "#%lu\n", t);
		this->t = t;
	}

	int m = (int)max(curr.values.size(), encoding.values.size());
	for (int i = 0; i < m*16; i++) {
		int value = encoding.get(i);
		if (value != curr.get(i)) {
			const char *id = nullptr;
			if (i < (int)nets.size()) {
				id = nets[i].c_str();
			} else if (i-(int)nets.size() < (int)nodes.size()) {
				id = nodes[i-(int)nets.size()].c_str();
			}

			if (id != nullptr) {
				fprintf(fvcd, "%c%s\n", values[value+1], id);
			}
		}
	}

	if (not error.empty()) {
		markers.push_back(pair<uint64_t, string>(t, error));
	}
}

void vcd::append(uint64_t t, boolean::cube encoding, boolean::cube strength, string error) {
	static const char values[4] = {'x','0','1','z'};
	if (t > this->t) {
		fprintf(fvcd, "#%lu\n", t);
		this->t = t;
	}

	int m = (int)max(curr.values.size(), encoding.values.size());
	for (int i = 0; i < m*16; i++) {
		int value = encoding.get(i);
		int drive = 2-strength.get(i);
		if (drive == 0) {
			value = 2;
		}
		if (value != curr.get(i)) {
			const char *id;
			if (i < (int)nets.size()) {
				id = nets[i].c_str();
			} else {
				id = nodes[i-(int)nets.size()].c_str();
			}

			fprintf(fvcd, "%c%s\n", values[value+1], id);
		}
	}

	if (not error.empty()) {
		markers.push_back(pair<uint64_t, string>(t, error));
	}
}


void vcd::close() {
	if (fvcd != nullptr) {
		fclose(fvcd);
		fvcd = nullptr;
	}
	if (fgtk != nullptr) {
		fprintf(fgtk, "*1.0 0");
		for (auto marker = markers.begin(); marker != markers.end(); marker++) {
			fprintf(fgtk, " %lu", marker->first);
		}
		fprintf(fgtk, "\n");
		char id = 'A';
		for (auto marker = markers.begin(); marker != markers.end(); marker++) {
			fprintf(fgtk, "[markername] %c %s\n", id++, marker->second.c_str());
		}
		markers.clear();

		fclose(fgtk);
		fgtk = nullptr;
	}
}
