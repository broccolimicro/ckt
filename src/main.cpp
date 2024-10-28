#include <common/standard.h>
#include <parse/parse.h>

#include "version.h"
#include "build.h"
#include "undo.h"
#include "sim.h"
#include "show.h"
#include "test.h"
#include "compare.h"

void print_help() {
	printf("Loom is a high level synthesis and simulation engine for self-timed circuits.\n");
	printf("\nUsage: lm [options] <command> [arguments]\n");
	printf("Execute a sub-command:\n");
	printf("  build         compile the behavioral specification\n");
	printf("  undo          decompile the circuit\n");
	printf("  sim           simulate the described circuit\n");
	printf("  test          elaborate the full state space to test the circuit\n");
	printf("  compare       ensure that two circuit descriptions match\n");
	printf("  show          render the described circuit in various ways\n");
	printf("\n");
	printf("  help          display this help text or more information about a command\n");
	printf("  version       display version information\n");
	
	printf("\nUse \"lm help <command>\" for more information about a command.\n");

	printf("\nGeneral Options:\n");
	printf(" -v,--verbose   display verbose messages\n");
	printf(" -d,--debug     display internal debugging messages\n");
	printf(" -p,--progress  display progress information\n");
}

void print_version() {
	printf("Loom %s\n", version);
	printf("Copyright (C) 2024 Broccoli, LLC.\n");
	printf("There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");
	printf("\n");
}

int main(int argc, char **argv) {
	configuration config;
	config.set_working_directory(argv[0]);

	if (argc <= 1) {
		print_help();
		return 0;
	}

	bool progress = false;
	bool debug = false;
	for (int i = 1; i < argc; i++)
	{
		string arg = argv[i];
		if (arg == "--verbose" or arg == "-v") {
			set_verbose(true);
		} else if (arg == "--debug" or arg == "-d") {
			set_debug(true);
			debug = true;
		} else if (arg == "--progress" or arg == "-p") {
			progress = true;
		} else if (arg == "build") {
			++i;
			return build_command(config, argc-i, argv+i, progress, debug);
		} else if (arg == "sim") {
			++i;
			return sim_command(config, argc-i, argv+i);
		} else if (arg == "undo") {
			++i;
			return undo_command(config, argc-i, argv+i, progress, debug);
		} else if (arg == "test") {
			++i;
			return test_command(config, argc-i, argv+i);
		} else if (arg == "compare") {
			++i;
			return compare_command(config, argc-i, argv+i, progress, debug);
		} else if (arg == "show") {
			++i;
			return show_command(config, argc-i, argv+i);
		} else if (arg == "version") {
			print_version();
			return 0;
		} else if (arg == "help") {
			if (++i >= argc) {
				print_help();
				return 0;
			}

			arg = argv[i];
			if (arg == "build") {
				build_help();
			} else if (arg == "sim") {
				sim_help();
			} else if (arg == "show") {
				show_help();
			} else if (arg == "undo") {
				undo_help();
			} else if (arg == "test") {
				test_help();
			} else if (arg == "compare") {
				compare_help();
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
	
