#include "unpack.h"

#include <common/standard.h>
#include <common/timer.h>
#include <common/text.h>

#include "weaver/unpacker.h"
#include "format.h"

void unpack_help() {
	printf("Usage: lm unpack [options] <file>\n");
	printf("Reverse the synthesis process.\n");
	printf("\nOptions:\n");
	printf(" --all          save all intermediate stages\n");
	printf(" -n,--nets      save the extracted netlist\n");
	printf(" -s,--size      save the extracted sized production rules\n");

	printf("\nSupported file formats:\n");
	printf(" *.gds                    layout file\n");
	printf(" *.spice,*.spi,*.sp,*.s   spice netlist\n");
}

int unpack_command(int argc, char **argv) {
	weaver::Project proj;
	if (proj.hasMod()) {
		readMod(proj);
	} else {
		// default to skywater 130
		proj.setTech("sky130");
	}

	loadAllFormats(proj);

	vector<weaver::Prototype> protos;

	Unpack unpacker(proj);

	for (int i = 0; i < argc; i++) {
		string arg = argv[i];

		if (arg == "--verbose" or arg == "-v") {
			set_verbose(true);
		} else if (arg == "--debug" or arg == "-d") {
			set_debug(true);
			unpacker.debug = true;
		} else if (arg == "--all") {
			unpacker.inclAll();
		} else if (arg == "-l" or arg == "--layout") {
			// This is really only for debugging
			unpacker.set(Unpack::LAYOUT);
		} else if (arg == "-n" or arg == "--nets") {
			unpacker.set(Unpack::NETS);
		} else if (arg == "-s" or arg == "--size") {
			unpacker.set(Unpack::SIZED);
		} else {
			protos.push_back(weaver::Prototype(arg));
		}
	}

	weaver::Program prgm;
	loadGlobalTypes(prgm);

	/*if (protos.empty()) {
		proj.incl(proj.modName);
		proj.load(prgm);
		unpacker.unpack(prgm);
	} else {
		for (auto i = protos.begin(); i != protos.end(); i++) {
			proj.incl(i->mod);
		}
		proj.load(prgm);
		for (auto i = protos.begin(); i != protos.end(); i++) {
			vector<weaver::TermId> curr = prgm.findTerms(*i);
			if (curr.empty()) {
				error("", "module not found for term '" + i->to_string() + "'", __FILE__, __LINE__);
			}
			for (auto j = curr.begin(); j != curr.end(); j++) {
				unpacker.unpack(prgm, *j);
			}
		}
	}*/

	if (unpacker.debug) {
		prgm.print();
	}

	proj.save(prgm);

	if (!is_clean()) {
		complete();
		return 1;
	}

	return 0;
}

