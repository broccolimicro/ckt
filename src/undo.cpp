#include "undo.h"

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
#include <ucs/variable.h>

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

void undo_help() {
	printf("Usage: lm undo [options] <file>\n");
	printf("Reverse the synthesis process.\n");

	printf("\nSupported file formats:\n");
	printf(" *.gds                    layout file\n");
	printf(" *.spice,*.spi,*.sp,*.s   spice netlist\n");
}

int undo_command(configuration &config, int argc, char **argv, bool progress, bool debug) {
	tokenizer tokens;
	
	string filename = "";
	string prefix = "";
	string format = "";
	string techPath = "";
	
	char *loom_tech = std::getenv("LOOM_TECH");
	string techDir;
	if (loom_tech != nullptr) {
		techDir = string(loom_tech);
	}
	string cellsDir = "cells";

	for (int i = 0; i < argc; i++) {
		string arg = argv[i];

		if (arg == "-o" or arg == "--out") {
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

			if (ext == "py") {
				techPath = arg;
			} else if (ext == "") {
				if (not techDir.empty()) {
					techPath = techDir + "/" + arg + "/" + arg+".py";
					cellsDir = techDir + "/" + arg + "/cells";
				} else {
					techPath = arg+".py";
				}
			} else {
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
	}

	if (filename == "") {
		printf("expected filename\n");
		return 0;
	}

	phy::Tech tech;
	if (not phy::loadTech(tech, techPath)) {
		cout << "techfile does not exist \'" + techPath + "\'." << endl;
		return 1;
	}
	phy::Library lib(tech, cellsDir);
	sch::Netlist net(tech);

	if (format == "gds") {
		import_library(lib, filename);
		extract(net, lib);
	}

	cout << export_netlist(net).to_string() << endl;

	if (!is_clean()) {
		complete();
		return 1;
	}

	return 0;
}

