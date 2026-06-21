#include "show.h"

#include <common/standard.h>
#include <common/message.h>

#include <chp/graph.h>
#include <hse/graph.h>
#include <hse/elaborator.h>
#include <prs/production_rule.h>
#include <prs/bubble.h>

#include <interpret_chp/export.h>
#include <interpret_hse/export.h>

#include "format.h"

struct ShowOptions {
	ShowOptions() {
		encodings = -1;

		proper = true;
		aggressive = false;
		process = true;
		labels = false;
		notations = false;
		horiz = false;
		states = false;
		petri = false;
		ghost = false;
	}

	~ShowOptions() {
	}

	int encodings;

	bool proper;
	bool aggressive;
	bool process;
	bool labels;
	bool notations;
	bool horiz;
	bool states;
	bool petri;
	bool ghost;
};

void show_help() {
	printf("Usage: lm show [options] file...\n");
	printf("Create visual representations of the circuit or behavior.\n");
	printf("\nOptions:\n");
	printf(" -o              Specify the output file name, formats other than 'dot'\n");
	printf("                 are passed onto graphviz dot for rendering\n");
	printf(" -l,--labels     Show the IDs for each place, transition, and arc\n");
	printf(" -lr,--leftright Render the graph from left to right\n");
	printf(" -e,--effective  Show the effective encoding of each place\n");
	printf(" -p,--predicate  Show the predicate of each place\n");
	printf(" -g,--ghost      Show the state annotations for the conditional branches\n");
	printf(" -r,--raw        Do not post-process the graph\n");
	printf(" -s,--sync       Render half synchronization actions\n");
}

void show(ShowOptions opts, weaver::Term &t, weaver::Variant &v, string outPath) {
	if (v.meta.dialect == "func") {
		chp::graph g = v.as<chp::graph>();
		if (opts.process) {
			g.post_process(opts.proper, opts.aggressive);
		}
		gvdot::render(outPath, chp::export_graph(g, opts.labels).to_string());
	} else if (v.meta.dialect == "proto") {
		hse::graph g = v.as<hse::graph>();
		if (opts.process) {
			g.post_process(opts.proper, opts.aggressive);
		}
		if (opts.states) {
			hse::graph sg = hse::to_state_graph(g, true);
			gvdot::render(outPath, hse::export_graph(sg, opts.horiz, opts.labels, opts.notations, opts.ghost, opts.encodings).to_string());
		} else if (opts.petri) {
			hse::graph pn = hse::to_petri_net(g, true);
			gvdot::render(outPath, hse::export_graph(pn, opts.horiz, opts.labels, opts.notations, opts.ghost, opts.encodings).to_string());
		} else {
			gvdot::render(outPath, hse::export_graph(g, opts.horiz, opts.labels, opts.notations, opts.ghost, opts.encodings).to_string());
		}
	}
}

void show(ShowOptions opts, weaver::Term &t, string outPath) {
	if (t.variants.empty()) {
		internal("", "dialect not defined for term '" + t.decl.name + "'", __FILE__, __LINE__);
		return;
	}
	for (auto i = t.variants.begin(); i != t.variants.end(); i++) {
		show(opts, t, *i, outPath);
	}
}

void show(ShowOptions opts, fs::path outPath, weaver::Program &prgm, weaver::TermId term=weaver::TermId()) {
	if (term.mod < 0) {
		for (term.mod = 0; term.mod < (int)prgm.mods.size(); term.mod++) {
			show(opts, outPath, prgm, term);
		}
	} else if (term.index < 0) {
		for (term.index = 0; term.index < (int)prgm.mods[term.mod].terms.size(); term.index++) {
			show(opts, outPath, prgm, term);
		}
	} else {
		weaver::Term &t = prgm.termAt(term);
		show(opts, t, outPath / (t.decl.name + ".png"));
	}
}

int show_command(int argc, char **argv) {
	weaver::Project proj;
	if (proj.hasMod()) {
		readMod(proj);
	}

	loadAllFormats(proj);

	vector<weaver::Prototype> protos;

	ShowOptions opts;

	for (int i = 0; i < argc; i++) {
		string arg = argv[i];
		if (arg == "--labels" || arg == "-l") {
			opts.labels = true;
		} else if (arg == "--notations" || arg == "-nt") {
			opts.notations = true;
		} else if (arg == "--leftright" || arg == "-lr") {
			opts.horiz = true;
		} else if (arg == "--effective" || arg == "-e") {
			opts.encodings = 1;
		} else if (arg == "--predicate" || arg == "-p") {
			opts.encodings = 0;
		} else if (arg == "--ghost" || arg == "-g") {
			opts.ghost = true;
		} else if (arg == "--raw" || arg == "-r") {
			opts.process = false;
		} else if (arg == "--nest" || arg == "-n") {
			opts.proper = false;
		} else if (arg == "--aggressive" || arg == "-ag") {
			opts.aggressive = true;
		} else if (arg == "-sg" or arg == "--states") {
			opts.states = true;
		} else if (arg == "-pn" or arg == "--petri") {
			opts.petri = true;
		} else {
			protos.push_back(weaver::Prototype(arg));
		}
	}

	if (protos.empty() and not proj.hasMod()) {
		printf("please initialize your module with the following.\n\nlm mod init my_module\n");
		return 1;
	}

	weaver::Program prgm;
	loadGlobalTypes(prgm);

	if (protos.empty()) {
		proj.incl(proj.modName);
	} else {
		for (auto j = protos.begin(); j != protos.end(); j++) {
			proj.incl(j->mod);
		}
	}

	proj.load(prgm);

	fs::create_directories(proj.rootDir / proj.BUILD / "dbg");
	if (protos.empty()) {
		for (auto i = prgm.mods.begin(); i != prgm.mods.end(); i++) {
			for (auto j = i->terms.begin(); j != i->terms.end(); j++) {
				show(opts, *j, proj.buildPath("dbg", j->decl.name+".png").string());
			}
		}
	} else {
		for (auto i = protos.begin(); i != protos.end(); i++) {
			vector<weaver::TermId> curr = prgm.findTerms(*i);
			if (curr.empty()) {
				error("", "module not found for term '" + i->to_string() + "'", __FILE__, __LINE__);
			}
			for (auto j = curr.begin(); j != curr.end(); j++) {
				show(opts, proj.workDir, prgm, *j);
			}
		}
	}

	complete();
	return is_clean();
}
