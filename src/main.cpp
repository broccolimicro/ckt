#include <common/standard.h>
#include <parse/parse.h>

#include "version.h"
#include "build.h"
//#include "unpack.h"
//#include "sim.h"
#include "show.h"
//#include "test.h"
//#include "compare.h"
#include "tech.h"

#include <filesystem>
#include <fstream>

#include "weaver/project.h"

namespace fs = std::filesystem;

void print_help() {
	printf("Loom is a high level synthesis and simulation engine for self-timed circuits.\n");
	printf("\nUsage: lm [options] <command> [arguments]\n");
	printf("Execute a sub-command:\n");
	printf("  build         compile the behavioral specification\n");
	printf("  unpack        decompile the structural specification\n");
	printf("  sim           simulate the described circuit\n");
	printf("  test          verify the behavior/structure of the circuit\n");
	printf("  compare       ensure that two circuit specifications match\n");
	printf("  show          visualize the described circuit\n");
	printf("\n");
	printf("  mod           manage this module\n");
	printf("  tech          manage the technology node and cell libraries\n");
	//printf("  depend        manage your project's dependencies\n");
	printf("\n");
	printf("  help          display this help text or more information about a command\n");
	printf("  version       display version information\n");
	
	printf("\nUse \"lm help <command>\" for more information about a command.\n");
}

void print_version() {
	printf("Loom %s\n", version);
	printf("Copyright (C) 2024 Broccoli, LLC.\n");
	printf("There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");
	printf("\n");
}

int main(int argc, char **argv) {
	if (argc <= 1) {
		print_help();
		return 0;
	}

	for (int i = 1; i < argc; i++) {
		string arg = argv[i];
		if (arg == "build") {
			++i;
			return build_command(argc-i, argv+i);
		} else if (arg == "unpack") {
			++i;
			//return unpack_command(argc-i, argv+i);
		} else if (arg == "sim") {
			++i;
			//return sim_command(argc-i, argv+i);
		} else if (arg == "test") {
			++i;
			//return test_command(argc-i, argv+i);
		} else if (arg == "compare") {
			++i;
			//return compare_command(argc-i, argv+i);
		} else if (arg == "show") {
			++i;
			return show_command(argc-i, argv+i);
		} else if (arg == "tech") {
			++i;
			return tech_command(argc-i, argv+i);
		} else if (arg == "version") {
			print_version();
			return 0;
		} else if (arg == "help" or arg == "-h" or arg == "--help") {
			if (++i >= argc) {
				print_help();
				return 0;
			}

			arg = argv[i];
			if (arg == "build") {
				build_help();
			} else if (arg == "unpack") {
				//unpack_help();
			} else if (arg == "sim") {
				//sim_help();
			} else if (arg == "test") {
				//test_help();
			} else if (arg == "compare") {
				//compare_help();
			} else if (arg == "show") {
				show_help();
			} else if (arg == "tech") {
				tech_help();
			} else {
				printf("unrecognized command '%s'\n", argv[i]);
				return 0;
			}
		} else {
			printf("unrecognized command '%s'\n", argv[i]);
		}
	}

	complete();
	return is_clean();
}
	
