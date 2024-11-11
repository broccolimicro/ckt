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

int compare_command(configuration &config, string techPath, string cellsDir, int argc, char **argv, bool progress, bool debug) {
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

	if (techPath.empty()) {
		printf("please provide a python techfile.\n");
		return 0;
	}

	phy::Tech tech;
	auto gdsFiles = files.find("gds");
	auto spiFiles = files.find("spi");
	if (gdsFiles != files.end() or spiFiles != files.end()) {
		if (not phy::loadTech(tech, techPath, cellsDir)) {
			cout << "techfile does not exist \'" + techPath + "\'." << endl;
			return 1;
		}
	}

	vector<sch::Netlist> spiNet;
	vector<vector<bool> > spiCanon;
	if (spiFiles != files.end()) {
		spiNet.resize(spiFiles->second.size(), sch::Netlist(tech));
		spiCanon.resize(spiFiles->second.size(), vector<bool>());
		tokenizer tokens;
		parse_spice::netlist::register_syntax(tokens);
		int idx = 0;
		for (auto path = spiFiles->second.begin(); path != spiFiles->second.end(); path++) {
			config.load(tokens, *path, "");
			tokens.increment(false);
			tokens.expect<parse_spice::netlist>();
			if (tokens.decrement(__FILE__, __LINE__))
			{
				parse_spice::netlist syntax(tokens);
				sch::import_netlist(syntax, spiNet[idx], &tokens);
			}
			tokens.reset();
			spiCanon[idx].resize(spiNet[idx].subckts.size(), false);
			idx++;
		}
	}

	if (gdsFiles != files.end()) {
		printf("GDS vs Spice...\n");
		for (auto path = gdsFiles->second.begin(); path != gdsFiles->second.end(); path++) {
			phy::Library gdsLib(tech);
			import_library(gdsLib, *path);
			if (gdsLib.macros.empty()) {
				continue;
			}

			sch::Netlist gdsNet(tech);
			extract(gdsNet, gdsLib);
			for (auto gds = gdsNet.subckts.begin(); gds != gdsNet.subckts.end(); gds++) {
				gds->cleanDangling(true);
				gds->combineDevices();
				gds->canonicalize();

				bool found = false;
				for (auto net = spiNet.begin(); net != spiNet.end(); net++) {
					int neti = net-spiNet.begin();
					for (auto spi = net->subckts.begin(); spi != net->subckts.end(); spi++) {
						int spii = spi-net->subckts.begin();
						if (spi->name == gds->name) {
							printf("\t%s(%s=%s)...[", gds->name.c_str(), path->c_str(), spiFiles->second[net-spiNet.begin()].c_str());
							fflush(stdout);
							if (not spiCanon[neti][spii]) {
								spi->cleanDangling(true);
								spi->combineDevices();
								spi->canonicalize();
								spiCanon[neti][spii] = true;
							}
							if (gds->compare(*spi) == 0) {
								printf("%sMATCH%s]\n", KGRN, KNRM);
							} else {
								printf("%sMISMATCH%s]\n", KRED, KNRM);
								if (debug) {
									spi->print();
									gds->print();
								}
							}
							found = true;
							break;
						}
					}
				}
				if (not found) {
					printf("\t%s...[%sNOT FOUND%s]\n", gds->name.c_str(), KYEL, KNRM);
				}
			}
		}
		printf("done\n\n");
	}

	if ((int)spiNet.size() > 1) {
		printf("Spice vs Spice...\n");
		for (auto n0 = spiNet.begin(); n0 != spiNet.end(); n0++) {
			int n0i = n0-spiNet.begin();
			for (auto n1 = std::next(n0); n1 != spiNet.end(); n1++) {
				int n1i = n1-spiNet.begin();
				for (auto s0 = n0->subckts.begin(); s0 != n0->subckts.end(); s0++) {
					int s0i = s0-n0->subckts.begin();
					for (auto s1 = n1->subckts.begin(); s1 != n1->subckts.end(); s1++) {
						int s1i = s1-n1->subckts.begin();
						if (s0->name == s1->name) {
							printf("\t%s(%s=%s)...[", s0->name.c_str(), spiFiles->second[n0-spiNet.begin()].c_str(), spiFiles->second[n1-spiNet.begin()].c_str());
							fflush(stdout);
							if (not spiCanon[n0i][s0i]) {
								s0->combineDevices();
								s0->canonicalize();
								spiCanon[n0i][s0i] = true;
							}
							if (not spiCanon[n1i][s1i]) {
								s1->combineDevices();
								s1->canonicalize();
								spiCanon[n1i][s1i] = true;
							}
							if (s0->compare(*s1) == 0) {
								printf("%sMATCH%s]\n", KGRN, KNRM);
							} else {
								printf("%sMISMATCH%s]\n", KRED, KNRM);
								if (debug) {
									s0->print();
									s1->print();
								}
							}
						}
					}
				}
			}
		}
		printf("done\n\n");
	}

	complete();
	return is_clean();
}
