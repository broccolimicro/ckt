#include "mod.h"

#include <common/standard.h>
#include <common/timer.h>
#include <common/text.h>

#include "format.h"

void mod_help() {
	printf("Usage: lm mod <command> [arguments]\n");
	printf("Manage the module. Sub-commands are:\n");
	printf("  init <name>     initialize a new module in this directory\n");
	printf("  vendor          download dependencies into the vendor directory\n");
	printf("  tidy            update the dependency list\n");
	printf("  show [proto...] list all terms found in the module\n");
}

int mod_command(int argc, char **argv) {
	if (argc == 0) {
		mod_help();
		return 0;
	}

	weaver::Project proj;
	if (proj.hasMod()) {
		readMod(proj);
	} else {
		// default to skywater 130
		proj.setTech("sky130");
	}

	loadAllFormats(proj);

	vector<weaver::Prototype> protos;
	vector<std::string> files;
	bool show = false;
	bool debug = false;

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
			writeMod(proj);
			return 0;
		} else if (arg == "vendor") {
			if (not proj.hasMod()) {
				printf("please initialize your module with the following.\n\n$ lm mod init my_module\n");
				return 1;
			}
			readMod(proj);
			proj.vendor();
			return 0;
		} else if (arg == "tidy") {
			proj.tidy();
			writeMod(proj);
			return 0;
		} else if (arg == "show") {
			show = true;
		} else if (arg == "-d" or arg == "--debug") {
			debug = true;
		} else if (show) {
			if (arg.rfind("fn:", 0) != string::npos) {
				protos.push_back(weaver::Prototype(arg.substr(3)));
			} else {
				files.push_back(arg);
			}
		} else {
			error("", "unrecognized command '" + arg + "'", __FILE__, __LINE__);
			complete();
			return is_clean();
		}
	}

	if (not show) {
		complete();
		return is_clean();
	}

	weaver::Program prgm;
	loadGlobalTypes(prgm);

	for (auto path : files) {
		proj.inclFile(path);
	}
	for (const auto &proto : protos) {
		proj.incl(proto.mod);
	}
	if (files.empty() and protos.empty()) {
		proj.incl(proj.modName);
	}
	proj.load(prgm);

	if (debug) {
		prgm.print();
	} else {
		for (auto i = prgm.begin(); i != prgm.end(); i = prgm.next(i)) {
			std::string name = prgm.getPrototype(i).to_string();
			printf("fn:%s\n", name.c_str());
		}
	}

	complete();
	return is_clean();
}
