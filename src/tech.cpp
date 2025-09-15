#include "tech.h"

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

#include "weaver/project.h"

#include <filesystem>
#include <chrono>
#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
using namespace std::chrono;

//const bool debug = false;

void tech_help() {
	printf("Usage: lm tech <command> [arguments]\n");
	printf("Manage the technology node and cell library. Sub-commands are:\n");
	printf("  list                display a list of available technology nodes\n");
	printf("  get                 print the current technology node\n");
	printf("  set <tech>          set the current technology node\n");
	//printf("  import <pdk>        import the technology rules and cell library from a process design kit\n");
	printf("  cells <cell.gds...>  import the cells into the cell library\n");
	//printf("  check [cell name...]    run DRC/LVS checks against your cells\n");
	//printf("  push [cell name...]     share your cell layouts with others using the same technology\n");
	//printf("  drop [cell name...]     delete these cells from your cell library\n");
}

int tech_cells_command(string workingDir, string techDir, string techPath, string cellsDir, int argc, char **argv) {
	vector<string> files;

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
			}

			if (ext != "gds") {
				printf("unsupported file format '%s'\n", ext.c_str());
				return 0;
			}
			files.push_back(arg);
		}
	}

	if (techPath.empty()) {
		printf("please provide a python techfile.\n");
		return 0;
	}

	phy::Tech tech(techPath, cellsDir);
	if (not phy::loadTech(tech)) {
		cout << "techfile does not exist \'" + techPath + "\'." << endl;
		return 1;
	}

	bool libFound = filesystem::exists(tech.lib);
	printf("Importing cells...\n");
	for (auto path = files.begin(); path != files.end(); path++) {
		printf("\t%s\n", path->c_str());
		phy::Library lib(tech);
		import_library(lib, *path);
		if (lib.macros.empty()) {
			continue;
		}

		sch::Netlist net;
		extract(net, lib);
		for (int i = 0; i < (int)net.subckts.size(); i++) {
			auto spi = net.subckts.begin()+i;
			auto gds = lib.macros.begin()+i;
			printf("\t\t%s -> ", spi->name.c_str());
			fflush(stdout);
			spi->cleanDangling(true);
			spi->combineDevices();
			spi->canonicalize();
			spi->name = "cell_" + sch::idToString(spi->id);
			gds->name = spi->name;
			printf("%s\n", spi->name.c_str());	

			if (not libFound) {
				filesystem::create_directory(tech.lib);
				libFound = true;
			}
		
			string cellPath = tech.lib + "/" + spi->name;
			export_layout(cellPath+".gds", *gds);
			export_lef(cellPath+".lef", *gds);
			export_spi(cellPath+".spi", tech, net, *spi);
		}
	}

	return 1;
}

/*int tech_import_command(string workingDir, string techDir, string techPath, string cellsDir, int argc, char **argv) {
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
			}

			if (ext != ""
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

	return 1;
}*/

int tech_command(int argc, char **argv) {
	if (argc == 0) {
		tech_help();
	}

	weaver::Project proj;
	if (proj.hasMod()) {
		proj.readMod();
	} else {
		printf("please initialize your module with the following.\n\nlm mod init my_module\n");
		return 1;
	}

	for (int i = 0; i < argc; i++) {
		string arg = argv[i];

		if (arg == "") {
			// placeholder for arguments
		} else if (arg == "list") {
			++i;
			auto techs = proj.listTech();
			for (auto i = techs.begin(); i != techs.end(); i++) {
				printf("%s\n", i->c_str());
			}
			return 0;
		} else if (arg == "set") {
			++i;
			if (i < argc) {
				proj.setTech(argv[i]);
			}
			proj.writeMod();
			return 0;
		} else if (arg == "get") {
			++i;
			printf("%s\n", proj.techName.c_str());
			return 0;
		} else if (arg == "cells") {
			++i;
			return tech_cells_command(proj.workDir, proj.techDir, proj.tech.path, proj.tech.lib, argc-i, argv+i);
		/*} else if (arg == "import") {
			++i;
			return tech_import_command(workingDir, techDir, techPath, cellsDir, argc-i, argv+i);*/
		} else {
			tech_help();
		}
	}

	complete();
	return is_clean();
}
