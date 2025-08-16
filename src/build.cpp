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

#include <chp/graph.h>
//#include <chp/simulator.h>
//#include <parse_chp/composition.h>
#include <interpret_chp/import.h>
#include <interpret_chp/export.h>

#include <hse/graph.h>
#include <hse/simulator.h>
#include <hse/encoder.h>
#include <hse/elaborator.h>
#include <hse/synthesize.h>
#include <interpret_hse/import.h>
#include <interpret_hse/export.h>
#include <interpret_hse/export_cli.h>

#include <prs/production_rule.h>
#include <prs/bubble.h>
#include <prs/synthesize.h>
#include <interpret_prs/import.h>
#include <interpret_prs/export.h>

#include <sch/Netlist.h>
#include <sch/Tapeout.h>
#include <sch/Placer.h>
#include <interpret_sch/import.h>
#include <interpret_sch/export.h>

#include <phy/Tech.h>
#include <phy/Script.h>
#include <phy/Layout.h>
#include <phy/Library.h>
#include <interpret_phy/import.h>
#include <interpret_phy/export.h>

//#include <parse_expression/expression.h>
//#include <parse_expression/assignment.h>
//#include <parse_expression/composition.h>
#include <interpret_boolean/export.h>
#include <interpret_boolean/import.h>
#include <interpret_arithmetic/export.h>
#include <interpret_arithmetic/import.h>

#include <chp/synthesize.h>
#include <flow/func.h>
#include <flow/module.h>
#include <flow/synthesize.h>
#include <interpret_flow/export.h>

#include <filesystem>

#include "weaver/builder.h"
#include "weaver/binder.h"
#include "format/dot.h"

#include "dialect.h"

namespace chp {

std::any factory(const parse::syntax *syntax, tokenizer *tokens) {
	graph g;
	if (syntax != nullptr) {
		import_chp(g, *(const parse_cog::composition *)syntax, tokens, true);
		//g.post_process(true);
	}
	return g;
}

}

namespace hse {

std::any factory(const parse::syntax *syntax, tokenizer *tokens) {
	graph g;
	if (syntax != nullptr) {
		import_hse(g, *(const parse_cog::composition *)syntax, tokens, true);
		g.post_process(true);
		g.check_variables();
	}
	return g;
}

}

namespace prs {

std::any factory(const parse::syntax *syntax, tokenizer *tokens) {
	prs::production_rule_set pr;
	if (syntax != nullptr) {
		prs::import_production_rule_set(*(const parse_prs::production_rule_set *)syntax, pr, -1, -1, prs::attributes(), 0, tokens, true);
	}
	return pr;
}

}

void build_help() {
	printf("\nUsage: lm build [options] <file>\n");
	printf("Synthesize the production rules that implement the behavioral description.\n");

	printf("\nOptions:\n");
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

int build_command(string workingDir, string techPath, string cellsDir, int argc, char **argv, bool progress, bool debug) {
	Build builder;
	builder.progress = progress;
	builder.debug = debug;
	builder.techPath = techPath;
	builder.cellsDir = cellsDir;

	tokenizer tokens;
	
	string filename = "";
	
	for (int i = 0; i < argc; i++) {
		string arg = argv[i];

		if (arg == "--logic") {
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
			string path = extractPath(arg);
			string opt = (arg.size() > path.size() ? arg.substr(path.size()+1) : "");

			filename = path;
		}
	}

	Timer totalTime;

	parse_ucs::function::registry.insert({"func", parse_ucs::language(&parse_cog::produce, &parse_cog::expect, &parse_cog::register_syntax)});
	//parse_ucs::function::registry.insert({"chp", parse_ucs::language(&parse_chp::produce, &parse_chp::expect, &parse_chp::register_syntax)});
	parse_ucs::function::registry.insert({"proto", parse_ucs::language(&parse_cog::produce, &parse_cog::expect, &parse_cog::register_syntax)});
	parse_ucs::function::registry.insert({"ckt", parse_ucs::language(&parse_prs::produce, &parse_prs::expect, &parse_prs::register_syntax)});
	//parse_ucs::function::registry.insert({"spice", parse_ucs::language(&parse_spice::produce, &parse_spice::expect, &parse_spice::register_syntax)});

	weaver::Term::pushDialect("func", chp::factory);
	weaver::Term::pushDialect("proto", hse::factory);
	weaver::Term::pushDialect("ckt", prs::factory);

	weaver::Binder::pushDialect("wv", wvFactory);
	weaver::Binder::pushDialect("cog", cogFactory);
	weaver::Binder::pushDialect("cogw", cogwFactory);
	weaver::Binder::pushDialect("prs", prsFactory);
	weaver::Binder::pushDialect("spi", spiFactory);
	weaver::Binder::pushDialect("gds", gdsFactory);

	weaver::Program prgm;
	loadGlobalTypes(prgm);

	weaver::Binder bd(prgm);
	bd.load(filename);
	builder.build(prgm);
	prgm.print();
	builder.emit(bd.findProjectRoot()+"/build", prgm);

	if (!is_clean()) {
		complete();
		return 1;
	}

	return 0;
}

