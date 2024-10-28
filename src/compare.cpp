#include "sim.h"

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

const bool debug = false;

void compare_help() {
	printf("Usage: lm compare [options] <file> <file> <tech.py>\n");
	printf("Verify that the two circuit files are the same.\n");

	printf("\nSupported file formats:\n");
	//printf(" *.chp                   communicating hardware processes\n");
	//printf(" *.hse                   handshaking expansions\n");
	//printf(" *.prs                   production rule set\n");
	//printf(" *.astg                  asynchronous signal transition graph\n");
	printf(" *.gds                   layout\n");
	printf(" *.spice,*.spi,*.sp,*.s  spice netlist\n");
}

int compare_command(configuration &config, int argc, char **argv) {
	string techPath = "";
	
	char *loom_tech = std::getenv("LOOM_TECH");
	string techDir;
	if (loom_tech != nullptr) {
		techDir = string(loom_tech);
	}
	string cellsDir = "cells";

	map<string, vector<string> > files;

	for (int i = 0; i < argc; i++) {
		string arg = argv[i];

		if (arg == "") {
			// placeholder for arguments
		} else {
			string path = extractPath(arg);
			string opt = (arg.size() > path.size() ? arg.substr(path.size()+1) : "");

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
					techPath = techDir + "/" + path + "/" + path+".py" + opt;
					cellsDir = techDir + "/" + path + "/cells";
				} else {
					techPath = path+".py" + opt;
				}
			} else {
				//string prefix = dot != string::npos ? arg.substr(0, dot) : arg;

				if (/*ext != "chp"
					and ext != "hse"
					and ext != "astg"
					and ext != "prs"
					and */ext != "spi"
					and ext != "gds") {
					printf("unsupported file format '%s'\n", ext.c_str());
					return 0;
				}
				auto pos = files.insert(pair<string, vector<string> >(ext, vector<string>()));
				pos.first->second.push_back(arg);
			}
		}
	}

	if ((int)files.size() < 2) {
		printf("expected at least two files at different levels of abstraction\n");
		return 0;
	}
	if (techPath.empty()) {
		printf("please provide a python techfile.\n");
		return 0;
	}

	phy::Tech tech;
	auto gdsFiles = files.find("gds");
	auto spiFiles = files.find("spi");
	if (gdsFiles != files.end() or spiFiles != files.end()) {
		if (not phy::loadTech(tech, techPath)) {
			cout << "techfile does not exist \'" + techPath + "\'." << endl;
			return 1;
		}
	}

	phy::Library gdsLib(tech, cellsDir);
	if (gdsFiles != files.end()) {
		for (auto path = gdsFiles->second.begin(); path != gdsFiles->second.end(); path++) {
			import_library(gdsLib, *path);
		}
	}

	sch::Netlist spiNet(tech);
	if (spiFiles != files.end()) {
		tokenizer tokens;
		tokens.register_token<parse::block_comment>(false);
		tokens.register_token<parse::line_comment>(false);
		parse_spice::netlist::register_syntax(tokens);
		for (auto path = spiFiles->second.begin(); path != spiFiles->second.end(); path++) {
			config.load(tokens, *path, "");
			tokens.increment(false);
			tokens.expect<parse_spice::netlist>();
			if (tokens.decrement(__FILE__, __LINE__))
			{
				parse_spice::netlist syntax(tokens);
				sch::import_netlist(syntax, spiNet, &tokens);
			}
			tokens.reset();
		}
		for (auto spi = spiNet.subckts.begin(); spi != spiNet.subckts.end(); spi++) {
			spi->canonicalize();
		}
	}

	sch::Netlist gdsNet(tech);
	if (not gdsLib.macros.empty()) {
		extract(gdsNet, gdsLib);
		for (auto gds = gdsNet.subckts.begin(); gds != gdsNet.subckts.end(); gds++) {
			gds->canonicalize();
		}
	}

	printf("Layout Versus Schematic...\n");
	for (auto gds = gdsNet.subckts.begin(); gds != gdsNet.subckts.end(); gds++) {
		printf("\t%s...[", gds->name.c_str());
		bool found = false;
		for (auto spi = spiNet.subckts.begin(); spi != spiNet.subckts.end(); spi++) {
			if (spi->name == gds->name) {
				if (gds->compare(*spi) == 0) {
					printf("%sMATCH%s]\n", KGRN, KNRM);
				} else {
					printf("%sDIFFER%s]\n", KRED, KNRM);
				}
				found = true;
				break;
			}
		}
		if (not found) {
			printf("%sNOT FOUND%s]\n", KYEL, KNRM);
		}
	}
	printf("done\n\n");

	complete();
	return is_clean();
}
