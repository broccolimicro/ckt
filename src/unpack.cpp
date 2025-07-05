#include "unpack.h"
#include "cli.h"

#include <common/standard.h>
#include <parse/parse.h>
#include <parse/default/block_comment.h>
#include <parse/default/line_comment.h>

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

#include <prs/production_rule.h>
#include <prs/bubble.h>
#include <prs/synthesize.h>
#include <interpret_prs/import.h>
#include <interpret_prs/export.h>

#include <sch/Netlist.h>
#include <sch/Tapeout.h>
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

#include <filesystem>
#include <chrono>
#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
using namespace std::chrono;

//printf(" -c             check for state conflicts that occur regardless of sense\n");
//	printf(" -cu            check for state conflicts that occur due to up-going transitions\n");
//	printf(" -cd            check for state conflicts that occur due to down-going transitions\n");


#ifdef GRAPHVIZ_SUPPORTED
namespace graphviz
{
	#include <graphviz/cgraph.h>
	#include <graphviz/gvc.h>
}
#endif

void unpack_help() {
	printf("Usage: lm unpack [options] <file>\n");
	printf("Reverse the synthesis process.\n");
	printf("\nOptions:\n");
	printf(" --all          save all intermediate stages\n");
	printf(" -n,--nets      save the extracted netlist\n");
	printf(" -s,--size      save the extracted sized production rules\n");

	printf("\nSupported file formats:\n");
	printf(" *.gds                    layout file\n");
	printf(" *.spice,*.spi,*.sp,*.s   spice netlist\n");
}

int unpack_command(configuration &config, string techPath, string cellsDir, int argc, char **argv, bool progress, bool debug) {
	tokenizer tokens;
	
	string filename = "";
	string prefix = "";
	string format = "";

	const int DO_LAYOUT = 0;
	const int DO_NETS = 1;
	const int DO_SIZED = 2;

	int stage = -1;

	bool doLayout = false;
	bool doNets = false;
	bool doSized = false;
	
	for (int i = 0; i < argc; i++) {
		string arg = argv[i];

		if (arg == "--all") {
			doNets = true;
			doSized = true;
		} else if (arg == "-l" or arg == "--layout") {
			// This is really only for debugging
			doLayout = true;
			set_stage(stage, DO_LAYOUT);
		} else if (arg == "-n" or arg == "--nets") {
			doNets = true;
			set_stage(stage, DO_NETS);
		} else if (arg == "-s" or arg == "--size") {
			doSized = true;
			set_stage(stage, DO_SIZED);
		} else if (arg == "-o" or arg == "--out") {
			if (++i >= argc) {
				printf("expected output prefix\n");
			}

			prefix = argv[i];
		} else {
			string path = extractPath(arg);

			size_t dot = path.find_last_of(".");
			string ext = "";
			if (dot != string::npos) {
				ext = path.substr(dot+1);
				if (ext == "spice"
					or ext == "sp"
					or ext == "s") {
					ext = "spi";
				}
			}

			filename = arg;
			format = ext;

			if (prefix == "") {
				prefix = filename.substr(0, dot);
			}

			if (ext != "chp"
				and ext != "hse"
				and ext != "astg"
				and ext != "prs"
				and ext != "spi"
				and ext != "gds") {
				printf("unrecognized file format '%s'\n", ext.c_str());
				return 0;
			}
		}
	}

	if (filename == "") {
		printf("expected filename\n");
		return 0;
	}

	phy::Tech tech;
	if (not phy::loadTech(tech, techPath, cellsDir)) {
		cout << "techfile does not exist \'" + techPath + "\'." << endl;
		return 1;
	}
	phy::Library lib(tech);
	sch::Netlist net;

	if (format == "gds") {
		import_library(lib, filename);

		if (doLayout) {
			export_library(prefix, prefix+"_ext.gds", lib);;
		}

		if (stage >= 0 and stage < DO_NETS) {
			complete();
			return 0;
		}

		extract(net, lib);

		if (doNets or stage < 0) {
			FILE *fout = fopen((prefix+"_ext.spi").c_str(), "w");
			fprintf(fout, "%s", sch::export_netlist(tech, net).to_string().c_str());
			fclose(fout);
		}
	}

	if (stage >= 0 and stage < DO_SIZED) {
		complete();
		return 0;
	}


	if (format == "spi") {
		parse_spice::netlist::register_syntax(tokens);
		config.load(tokens, filename, "");

		tokens.increment(false);
		tokens.expect<parse_spice::netlist>();
		if (tokens.decrement(__FILE__, __LINE__))
		{
			parse_spice::netlist syntax(tokens);
			sch::import_netlist(tech, net, syntax, &tokens);
		}
	}

	if (format == "gds"
		or format == "spi") {
		for (auto ckt = net.subckts.begin(); ckt != net.subckts.end(); ckt++) {
			prs::production_rule_set pr = prs::extract_rules(tech, *ckt);

			if (doSized or stage < 0) {
				FILE *fout = fopen((prefix+"_"+ckt->name+"_ext.prs").c_str(), "w");
				fprintf(fout, "%s", export_production_rule_set(pr).to_string().c_str());
				fclose(fout);
			}
		}
	}

	if (!is_clean()) {
		complete();
		return 1;
	}

	return 0;
}

