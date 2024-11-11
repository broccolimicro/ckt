#include <common/standard.h>
#include <parse/parse.h>

#include "version.h"
#include "build.h"
#include "unpack.h"
#include "sim.h"
#include "show.h"
#include "test.h"
#include "compare.h"
#include "tech.h"

#include <filesystem>
#include <fstream>

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
	printf("  tech          manage the technology node and cell libraries\n");
	//printf("  depend        manage your project's dependencies\n");
	printf("\n");
	printf("  help          display this help text or more information about a command\n");
	printf("  version       display version information\n");
	
	printf("\nUse \"lm help <command>\" for more information about a command.\n");

	printf("\nGeneral Options:\n");
	printf(" -h,--help        display this help message\n");
	printf(" -v,--verbose     display verbose messages\n");
	printf(" -d,--debug       display internal debugging messages\n");
	printf(" -p,--progress    display progress information\n");
	printf("\n");
	printf(" -t,--tech <techfile>    manually specify the technology file and arguments\n");
	printf(" -c,--cells <celldir>    manually specify the cell directory\n");
}

void print_version() {
	printf("Loom %s\n", version);
	printf("Copyright (C) 2024 Broccoli, LLC.\n");
	printf("There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");
	printf("\n");
}

int main(int argc, char **argv) {
	std::filesystem::path current = std::filesystem::current_path();

	configuration config;
	config.set_working_directory(argv[0]);

	if (argc <= 1) {
		print_help();
		return 0;
	}

	char *loom_tech = std::getenv("LOOM_TECH");
	string techDir = "/usr/local/share/tech";
	if (loom_tech != nullptr) {
		techDir = string(loom_tech);
		if ((int)techDir.size() > 1 and techDir.back() == '/') {
			techDir.pop_back();
		}
	}
	string techPath;
	string cellsDir;
	bool manualCells = false;

	if (not techDir.empty()) {
		string tech = "sky130";
		std::filesystem::path search = current;
		while (not search.empty()) {
			std::ifstream fptr(search / "lm.mod");
			if (fptr) {
				tech = "";
				getline(fptr, tech, '\n');
				fptr.close();
				break;
			} else if (search.parent_path() == search) {
				break;
			}
			search = search.parent_path();
		}

		techPath = techDir + "/" + tech + "/tech.py";
		cellsDir = techDir + "/" + tech + "/cells";
	} else {
		techPath = "";
		cellsDir = "cells";
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
		} else if (arg == "--tech" or arg == "-t") {
			if (++i >= argc) {
				printf("expected path to tech file.\n");
				return 0;
			}
			arg = argv[i];

			string path = extractPath(arg);
			string opt = (arg.size() > path.size() ? arg.substr(path.size()+1) : "");

			size_t dot = path.find_last_of(".");
			string ext = "";
			if (dot != string::npos) {
				ext = path.substr(dot+1);
			}

			if (ext == "py") {
				techPath = arg;
				if (not manualCells) {
					cellsDir = "cells";
				}
			} else if (ext == "") {
				if (not techDir.empty()) {
					techPath = techDir + "/" + path + "/tech.py" + opt;
					if (not manualCells) {
						cellsDir = techDir + "/" + path + "/cells";
					}
				} else {
					techPath = path+".py" + opt;
					if (not manualCells) {
						cellsDir = "cells";
					}
				}
			}
		} else if (arg == "--cells" or arg == "-c") {
			if (++i >= argc) {
				printf("expected path to cell directory.\n");
				return 0;
			}

			cellsDir = argv[i];
			manualCells = true;
		} else if (arg == "build") {
			++i;
			return build_command(config, techPath, cellsDir, argc-i, argv+i, progress, debug);
		} else if (arg == "unpack") {
			++i;
			return unpack_command(config, techPath, cellsDir, argc-i, argv+i, progress, debug);
		} else if (arg == "sim") {
			++i;
			return sim_command(config, techPath, cellsDir, argc-i, argv+i);
		} else if (arg == "test") {
			++i;
			return test_command(config, techPath, cellsDir, argc-i, argv+i);
		} else if (arg == "compare") {
			++i;
			return compare_command(config, techPath, cellsDir, argc-i, argv+i, progress, debug);
		} else if (arg == "show") {
			++i;
			return show_command(config, techPath, cellsDir, argc-i, argv+i);
		} else if (arg == "tech") {
			++i;
			return tech_command(config, techDir, techPath, cellsDir, argc-i, argv+i, progress, debug);
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
				unpack_help();
			} else if (arg == "sim") {
				sim_help();
			} else if (arg == "test") {
				test_help();
			} else if (arg == "compare") {
				compare_help();
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
	
