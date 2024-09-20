#include <common/standard.h>
#include <parse/parse.h>

#include "build.h"
#include "sim.h"
#include "show.h"
#include "verify.h"

void print_help() {
	printf("Loom is a high level synthesis and simulation engine for self-timed circuits.\n");
	printf("\nUsage: lm [options] <file>\n");
	printf("Synthesize the production rules that implement the behavioral description.\n");

	printf("\nOptions:\n");
	printf(" --logic <family>      pick the logic family you want to synthesize to. The following logic families are supported:\n");
	printf("         raw           do not require inverting logic\n");
	printf("         cmos          require inverting logic (default)\n");
	printf("\n");
	printf(" --all          save all intermediate stages\n");
	printf(" -o,--out       set the filename prefix for the saved intermediate stages\n\n");
	printf(" -g,--graph     save the elaborated astg\n");
	printf(" -c,--conflicts print the conflicts to stdout\n");
	printf(" -e,--encode    save the complete state encoded astg\n");
	printf(" -r,--rules     save the synthesized production rules\n");
	printf(" -b,--bubble    save the bubble reshuffled production rules\n");
	printf(" -s,--size      save the sized production rules\n");

	printf("\nSupported file formats:\n");
	printf(" *.chp          communicating hardware processes\n");
	printf(" *.hse          handshaking expansions\n");
	printf(" *.prs          production rules\n");
	printf(" *.astg         asynchronous signal transition graph\n");

	printf("\nUsage: lm [options] <command> [arguments]\n");
	printf("Execute a sub-command:\n");
	printf("  sim           simulate the described circuit\n");
	printf("  show          render the described circuit in various ways\n");
	printf("  verify        elaborate the full state space to verify the circuit\n");
	printf("\nUse \"lm help <command>\" for more information about a command.\n");

	printf("\nGeneral Options:\n");
	printf(" -h,--help      display this information\n");
	printf("    --version   display version information\n");
	printf(" -v,--verbose   display verbose messages\n");
	printf(" -d,--debug     display internal debugging messages\n");
	printf(" -p,--progress  display progress information\n");
}

void print_version() {
	printf("Loom 1.0.0\n");
	printf("Copyright (C) 2020 Cornell University.\n");
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
	for (int i = 1; i < argc; i++)
	{
		string arg = argv[i];
		if (arg == "--help" or arg == "-h") {
			print_help();
			return 0;
		} else if (arg == "--version") {
			print_version();
			return 0;
		} else if (arg == "--verbose" or arg == "-v") {
			set_verbose(true);
		} else if (arg == "--debug" or arg == "-d") {
			set_debug(true);
		} else if (arg == "--progress" or arg == "-p") {
			progress = true;
		} else if (arg == "sim") {
			++i;
			return sim_command(config, argc-i, argv+i);
		} else if (arg == "show") {
			++i;
			return show_command(config, argc-i, argv+i);
		} else if (arg == "verify") {
			++i;
			return verify_command(config, argc-i, argv+i);
		} else if (arg == "help") {
			if (++i >= argc) {
				print_help();
				return 0;
			}

			arg = argv[i];
			if (arg == "sim") {
				sim_help();
			} else if (arg == "show") {
				show_help();
			} else {
				printf("unrecognized command '%s'\n", argv[i]);
				return 0;
			}
		} else {
			return build_command(config, argc-i, argv+i, progress);
		}
	}

	complete();
	return is_clean();
}
	
