#include "mod.h"

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

void mod_help() {
	printf("Usage: lm mod <command> [arguments]\n");
	printf("Manage the module. Sub-commands are:\n");
	printf("  init <name>     initialize a new module in this directory\n");
	printf("  vendor          download dependencies into the vendor directory\n");
	printf("  tidy            update the dependency list\n");
}

int mod_command(int argc, char **argv) {
	if (argc == 0) {
		mod_help();
	}

	weaver::Project proj;

	for (int i = 0; i < argc; i++) {
		string arg = argv[i];

		if (arg == "init") {
			++i;
			if (i >= argc) {
				printf("error: expected module name\n");
				printf("$ lm mod init my_module\n");
				return 1;
			}
			proj.rootDir = proj.workDir;
			proj.modName = argv[i];
			proj.setTech("sky130");
			proj.writeMod();
			return 0;
		} else if (arg == "vendor") {
			if (not proj.hasMod()) {
				printf("please initialize your module with the following.\n\n$ lm mod init my_module\n");
				return 1;
			}
			proj.readMod();
			proj.vendor();
			return 0;
		} else if (arg == "tidy") {
			proj.tidy();
			proj.writeMod();
			return 0;
		} else {
			mod_help();
		}
	}

	complete();
	return is_clean();
}
