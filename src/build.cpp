#include "build.h"
#include "cli.h"

#include <common/standard.h>
#include <common/timer.h>
#include <common/text.h>
#include <parse/parse.h>
#include <parse/default/block_comment.h>
#include <parse/default/line_comment.h>
#include <parse/default/new_line.h>

#include <parse_ucs/source.h>
#include <parse_astg/factory.h>
#include <parse_cog/factory.h>
#include <parse_chp/factory.h>
#include <parse_prs/factory.h>
#include <parse_spice/factory.h>

#include "weaver/builder.h"
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

void build_help() {
	printf("\nUsage: lm build [options] <file>\n");
	printf("Synthesize the production rules that implement the behavioral description.\n");

	printf("\nOptions:\n");
	printf(" -v,--verbose     display verbose messages\n");
	printf(" -d,--debug       display internal debugging messages\n");
	printf("    --flow_html   enable HTML table output in debug mode (requires --debug)\n");
	printf(" -h,--help        display this help text\n");
	printf(" -p,--progress    display progress information\n");
	printf("\n");
	printf(" -t,--tech <techfile>    manually specify the technology file and arguments\n");
	printf(" -c,--cells <celldir>    manually specify the cell directory\n");
	printf("\n");
	printf(" --logic <family>      select the logic family for synthesis:\n");
	printf("         raw           do not require inverting logic\n");
	printf("         cmos          require inverting logic (default)\n");
	printf("\n");

	printf(" --timing <family>     select the timing family for synthesis:\n");
	printf("         mixed         do not constrain the timing family (default\n");
	printf("         qdi           strictly use quasi-delay insensitive handshakes\n");
	printf("         clocked       strictly use clocked val-rdy logic\n");
	printf("\n");

	printf(" --all          save all intermediate stages\n");
	printf(" -o,--out       set the filename prefix for the saved intermediate stages\n\n");
	printf(" -g,--graph     save the elaborated astg\n");
	printf(" -c,--conflicts print the conflicts to stdout\n");
	printf(" -e,--encode    save the complete state encoded astg\n");
	printf(" -r,--rules     save the synthesized production rules\n");
	printf(" -b,--bubble    save the bubble reshuffled production rules\n");
	printf(" -k,--keepers   save the production rules with added state-holding elements\n");
	printf(" -s,--size      save the sized production rules\n");
	printf(" -n,--nets      save the generated netlist\n");
	printf(" -m,--map       save the netlist split into cells\n");
	printf(" -l,--cells     save the cell layouts\n");
	printf(" -p,--place     save the cell placements\n");

	printf("\nSupported file formats:\n");
	printf(" *.cog          a wire-level programming language\n");
	printf(" *.chp          a data-level process calculi called Communicating Hardware Processes\n");
	printf(" *.hse          a wire-level process calculi called Hand-Shaking Expansions\n");
	printf(" *.prs          production rules\n");
	printf(" *.astg         asynchronous signal transition graph\n");
}

int build_command(int argc, char **argv) {
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

	Build builder(proj);
	
	bool manualCells = false;
	for (int i = 0; i < argc; i++) {
		string arg = argv[i];

		if (arg == "--verbose" or arg == "-v") {
			set_verbose(true);
		} else if (arg == "--debug" or arg == "-d") {
			set_debug(true);
			builder.debug = true;
		} else if (arg == "--flow_html") {
			if (!builder.debug) {
				printf("warning: --flow_html flag requires --debug to be enabled, ignoring\n");
			} else {
				builder.format_expressions_as_html_table = true;
			}
		} else if (arg == "-h" or arg == "--help") {
			build_help();
			return 0;

		} else if (arg == "--progress" or arg == "-p") {
			builder.progress = true;
		} else if (arg == "--tech" or arg == "-t") {
			if (++i >= argc) {
				printf("expected path to tech file.\n");
				return 0;
			}
			proj.setTechPath(argv[i], not manualCells);
		} else if (arg == "--cells" or arg == "-c") {
			if (++i >= argc) {
				printf("expected path to cell directory.\n");
				return 0;
			}
			proj.setCellsDir(argv[i]);
			manualCells = true;
		} else if (arg == "--logic") {
			if (++i >= argc) {
				printf("error: expected logic family (raw, cmos)\n");
			}
			arg = argv[i];

			if (arg == "raw") {
				builder.logic = Build::LOGIC_RAW;
			} else if (arg == "cmos") {
				builder.logic = Build::LOGIC_CMOS;
			} else if (arg == "adiabatic") {
				// This is not an officially supported logic family
				// It is here for experimental purposes
				builder.logic = Build::LOGIC_ADIABATIC;
			} else {
				cerr << "ERROR: Unsupported logic family '" << argv[i] << "' (expected raw/cmos)" << endl;
				return is_clean();
			}
		} else if (arg == "--timing") {
			if (++i >= argc) {
				printf("error: expected timing family (mixed, qdi, clocked)\n");
			}
			arg = argv[i];

			if (arg == "mixed") {
				builder.timing = Build::TIMING_MIXED;
			} else if (arg == "qdi") {
				builder.timing = Build::TIMING_QDI;
			} else if (arg == "clocked") {
				// This is not an officially supported timing family
				// It is here for experimental purposes
				builder.timing = Build::TIMING_CLOCKED;
			} else {
				printf("error: unsupported timing family '%s' (expected mixed, qdi, or clocked)\n", argv[i]);
				return is_clean();
			}
		} else if (arg == "--all") {
			builder.inclAll();
		} else if (arg == "--pre") {
			builder.doPreprocess = true;
		} else if (arg == "--post") {
			builder.doPostprocess = true;
		} else if (arg == "-g" or arg == "--graph") {
			builder.set(Build::ELAB);
		} else if (arg == "-c" or arg == "--conflict") {
			builder.set(Build::CONFLICTS);
		} else if (arg == "-e" or arg == "--encode") {
			builder.set(Build::ENCODE);
		} else if (arg == "-r" or arg == "--rules") {
			builder.set(Build::RULES);
		} else if (arg == "-b" or arg == "--bubble") {
			builder.set(Build::BUBBLE);
		} else if (arg == "-k" or arg == "--keepers") {
			builder.set(Build::KEEPERS);
		} else if (arg == "-s" or arg == "--size") {
			builder.set(Build::SIZE);
		} else if (arg == "-n" or arg == "--nets") {
			builder.set(Build::NETS);
		} else if (arg == "-m" or arg == "--map") {
			builder.set(Build::MAP);
		} else if (arg == "-l" or arg == "--cells") {
			builder.set(Build::CELLS);
		} else if (arg == "-p" or arg == "--place") {
			builder.set(Build::PLACE);
		} else if (arg == "-x" or arg == "--route") {
			builder.set(Build::ROUTE);
		} else if (arg == "+g" or arg == "++graph") {
			builder.incl(Build::ELAB);
		} else if (arg == "+c" or arg == "++conflict") {
			builder.incl(Build::CONFLICTS);
		} else if (arg == "+e" or arg == "++encode") {
			builder.incl(Build::ENCODE);
		} else if (arg == "+r" or arg == "++rules") {
			builder.incl(Build::RULES);
		} else if (arg == "+b" or arg == "++bubble") {
			builder.incl(Build::BUBBLE);
		} else if (arg == "+k" or arg == "++keepers") {
			builder.incl(Build::KEEPERS);
		} else if (arg == "+s" or arg == "++size") {
			builder.incl(Build::SIZE);
		} else if (arg == "+n" or arg == "++nets") {
			builder.incl(Build::NETS);
		} else if (arg == "+m" or arg == "++map") {
			builder.incl(Build::MAP);
		} else if (arg == "+l" or arg == "++cells") {
			builder.incl(Build::CELLS);
		} else if (arg == "+p" or arg == "++place") {
			builder.incl(Build::PLACE);
		} else if (arg == "+x" or arg == "++route") {
			builder.incl(Build::ROUTE);
		} else if (arg == "--no-cells") {
			builder.noCells = true;
		} else if (arg == "--no-ghosts") {
			builder.noGhosts = true;
		} else {
			protos.push_back(parseProto(proj, arg));
		}
	}

	Timer totalTime;

	weaver::Program prgm;
	loadGlobalTypes(prgm);

	// Create debug dir if it does not already exist
	std::filesystem::path debugDirPath = proj.rootDir / proj.BUILD / "dbg";
	if (!std::filesystem::exists(debugDirPath)) {
		if (!fs::create_directories(debugDirPath)) {
			string debugDirPathStr = debugDirPath.string();
			printf("error: %s does not exist & cannot be created\n", debugDirPathStr.c_str());
			return 1;
		}
	}

	if (protos.empty()) {
		proj.incl("top.wv");
		proj.load(prgm);
		builder.build(prgm);
	} else {
		for (auto i = protos.begin(); i != protos.end(); i++) {
			proj.incl(i->path);
		}
		proj.load(prgm);
		for (auto i = protos.begin(); i != protos.end(); i++) {
			vector<weaver::TermId> curr = findProto(prgm, *i);
			if (curr.empty()) {
				error("", "module not found for term '" + i->to_string() + "'", __FILE__, __LINE__);
			}
			for (auto j = curr.begin(); j != curr.end(); j++) {
				builder.build(prgm, *j);
			}
		}
	}

	if (builder.debug) {
		prgm.print();
	}

	proj.save(prgm);

	if (!is_clean()) {
		complete();
		return 1;
	}

	return 0;
}

