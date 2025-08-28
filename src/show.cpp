#include "show.h"

#include <common/standard.h>
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

int show_command(int argc, char **argv) {
	string filename = "";
	string term = "";

	int encodings = -1;

	bool proper = true;
	bool aggressive = false;
	bool process = true;
	bool labels = false;
	bool notations = false;
	bool horiz = false;
	bool states = false;
	bool petri = false;
	bool ghost = false;
	
	for (int i = 0; i < argc; i++) {
		string arg = argv[i];
		if (arg == "--labels" || arg == "-l") {
			labels = true;
		} else if (arg == "--notations" || arg == "-nt") {
			notations = true;
		} else if (arg == "--leftright" || arg == "-lr") {
			horiz = true;
		} else if (arg == "--effective" || arg == "-e") {
			encodings = 1;
		} else if (arg == "--predicate" || arg == "-p") {
			encodings = 0;
		} else if (arg == "--ghost" || arg == "-g") {
			ghost = true;
		} else if (arg == "--raw" || arg == "-r") {
			process = false;
		} else if (arg == "--nest" || arg == "-n") {
			proper = false;
		} else if (arg == "--aggressive" || arg == "-ag") {
			aggressive = true;
		} else if (arg == "-sg" or arg == "--states") {
			states = true;
		} else if (arg == "-pn" or arg == "--petri") {
			petri = true;
		} else {
			filename = argv[i];
			size_t pos = filename.find_last_of(':');
			if (pos != string::npos) {
				term = filename.substr(pos+1);
				filename = filename.substr(0, pos);
			}
		}
	}

	parse_ucs::function::registry.insert({"func", parse_ucs::language(&parse_cog::produce, &parse_cog::expect, &parse_cog::register_syntax)});
	//parse_ucs::function::registry.insert({"chp", parse_ucs::language(&parse_chp::produce, &parse_chp::expect, &parse_chp::register_syntax)});
	parse_ucs::function::registry.insert({"proto", parse_ucs::language(&parse_cog::produce, &parse_cog::expect, &parse_cog::register_syntax)});
	parse_ucs::function::registry.insert({"circ", parse_ucs::language(&parse_prs::produce, &parse_prs::expect, &parse_prs::register_syntax)});
	//parse_ucs::function::registry.insert({"spice", parse_ucs::language(&parse_spice::produce, &parse_spice::expect, &parse_spice::register_syntax)});

	weaver::Term::pushDialect("func", factoryCog);
	weaver::Term::pushDialect("proto", factoryCogw);
	weaver::Term::pushDialect("circ", factoryPrs);

	weaver::Project proj;
	if (proj.hasMod()) {
		proj.readMod();
	} else if (term.empty()) {
		printf("please initialize your module with the following.\n\nlm mod init my_module\n");
		return 1;
	}

	proj.pushFiletype("", "wv", "", readWv, loadWv);
	proj.pushFiletype("func", "cog", "", readCog, loadCog);
	proj.pushFiletype("proto", "cogw", "", readCog, loadCogw);
	proj.pushFiletype("circ", "prs", "ckt", readPrs, loadPrs, writePrs);
	proj.pushFiletype("spice", "spi", "spi", readSpice, loadSpice, writeSpice);
	proj.pushFiletype("verilog", "v", "rtl", nullptr, nullptr, writeVerilog);
	proj.pushFiletype("layout", "gds", "gds", nullptr, nullptr, writeGds);
	proj.pushFiletype("func", "astg", "state", readAstg, loadAstg, writeAstg);
	proj.pushFiletype("proto", "astgw", "state", readAstg, loadAstgw, writeAstgw);

	weaver::Program prgm;
	loadGlobalTypes(prgm);

	if (filename.empty()) {
		filename = "top.wv";
	}

	if (fs::path(filename).extension().empty()) {
		filename += ".wv";
	}

	proj.incl(filename);
	proj.load(prgm);

	if (term.empty()) {
		fs::create_directories(proj.rootDir / proj.BUILD / "dbg");
	}

	for (auto i = prgm.mods.begin(); i != prgm.mods.end(); i++) {
		for (auto j = i->terms.begin(); j != i->terms.end(); j++) {
			if (j->kind < 0) {
				printf("internal:%s:%d: dialect not defined for term '%s'\n", __FILE__, __LINE__, j->decl.name.c_str());
				continue;
			}
			if (not term.empty() and j->decl.name != term) {
				continue;
			}

			string outPath = proj.workDir / (j->decl.name + ".png");
			if (term.empty()) {
				outPath = proj.buildPath("dbg", j->decl.name+".png").string();
			}
			if (j->dialect().name == "func") {
				gvdot::render(outPath, chp::export_graph(std::any_cast<const chp::graph&>(j->def), labels).to_string());
			} else if (j->dialect().name == "proto") {
				hse::graph &g = std::any_cast<hse::graph&>(j->def);
				if (states) {
					hse::graph sg = hse::to_state_graph(g, true);
					gvdot::render(outPath, hse::export_graph(sg, horiz, labels, notations, ghost, encodings).to_string());
				} else if (petri) {
					hse::graph pn = hse::to_petri_net(g, true);
					gvdot::render(outPath, hse::export_graph(pn, horiz, labels, notations, ghost, encodings).to_string());
				} else {
					gvdot::render(outPath, hse::export_graph(g, horiz, labels, notations, ghost, encodings).to_string());
				}
			}
		}
	}

	complete();
	return is_clean();
}
