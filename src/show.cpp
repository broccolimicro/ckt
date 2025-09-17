#include "show.h"

#include <common/standard.h>
#include <common/message.h>
#include <parse/parse.h>
#include <parse/default/block_comment.h>
#include <parse/default/line_comment.h>

#include <parse_ucs/source.h>
#include <parse_astg/factory.h>
#include <parse_cog/factory.h>
#include <parse_chp/factory.h>
#include <parse_prs/factory.h>
#include <parse_spice/factory.h>

#include <chp/graph.h>
#include <hse/graph.h>
#include <hse/elaborator.h>
#include <prs/production_rule.h>
#include <prs/bubble.h>

#include "weaver/project.h"
#include "weaver/cli.h"

#include "format/dot.h"
#include "format/cog.h"
#include "format/spice.h"
#include "format/gds.h"
#include "format/verilog.h"
#include "format/prs.h"
#include "format/wv.h"
#include "format/astg.h"

#include <interpret_chp/export.h>
#include <interpret_hse/export.h>

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

void show(ShowOptions opts, weaver::Term &t, string outPath) {
	if (t.kind < 0) {
		internal("", "dialect not defined for term '" + t.decl.name + "'", __FILE__, __LINE__);
		return;
	}
	if (t.dialect().name == "func") {
		gvdot::render(outPath, chp::export_graph(t.as<chp::graph>(), opts.labels).to_string());
	} else if (t.dialect().name == "proto") {
		hse::graph &g = t.as<hse::graph>();
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
	parse_ucs::function::registry.insert({"func", parse_ucs::language(&parse_cog::produce, &parse_cog::expect, &parse_cog::register_syntax)});
	parse_ucs::function::registry.insert({"proto", parse_ucs::language(&parse_cog::produce, &parse_cog::expect, &parse_cog::register_syntax)});
	parse_ucs::function::registry.insert({"circ", parse_ucs::language(&parse_prs::produce, &parse_prs::expect, &parse_prs::register_syntax)});

	weaver::Term::pushDialect("func", factoryCog);
	weaver::Term::pushDialect("proto", factoryCogw);
	weaver::Term::pushDialect("circ", factoryPrs);

	weaver::Project proj;
	if (proj.hasMod()) {
		proj.readMod();
	}

	proj.pushFiletype("", "wv", "", readWv, loadWv);
	proj.pushFiletype("func", "cog", "", readCog, loadCog);
	proj.pushFiletype("proto", "cogw", "", readCog, loadCogw);
	proj.pushFiletype("circ", "prs", "ckt", readPrs, loadPrs, writePrs);
	proj.pushFiletype("spice", "spi", "spi", readSpice, loadSpice, writeSpice);
	proj.pushFiletype("verilog", "v", "rtl", nullptr, nullptr, writeVerilog);
	proj.pushFiletype("layout", "gds", "gds", nullptr, loadGds, writeGds);
	proj.pushFiletype("func", "astg", "state", readAstg, loadAstg, writeAstg);
	proj.pushFiletype("proto", "astgw", "state", readAstg, loadAstgw, writeAstgw);

	vector<Proto> protos;

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
			protos.push_back(parseProto(proj, arg));
		}
	}

	if (protos.empty() and not proj.hasMod()) {
		printf("please initialize your module with the following.\n\nlm mod init my_module\n");
		return 1;
	}

	weaver::Program prgm;
	loadGlobalTypes(prgm);

	if (protos.empty()) {
		proj.incl("top.wv");
	} else {
		for (auto j = protos.begin(); j != protos.end(); j++) {
			proj.incl(j->path);
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
			vector<weaver::TermId> curr = findProto(prgm, *i);
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
