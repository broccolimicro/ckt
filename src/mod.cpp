#include "mod.h"
#include "cli.h"

#include <common/standard.h>
#include <common/timer.h>
#include <common/text.h>
#include <parse/parse.h>
#include <parse/default/block_comment.h>
#include <parse/default/line_comment.h>
#include <parse/default/new_line.h>

#include <parse_ucs/source.h>
#include <parse_astg/factory.h>
#include <parse_cog/factory.h>
#include <parse_chp/factory.h>
#include <parse_prs/factory.h>
#include <parse_spice/factory.h>

#include "weaver/unpacker.h"
#include "weaver/project.h"
#include "weaver/cli.h"

#include "format/cog.h"
#include "format/spice.h"
#include "format/gds.h"
#include "format/verilog.h"
#include "format/prs.h"
#include "format/wv.h"
#include "format/astg.h"

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

	parse_ucs::function::registry.insert({"func", parse_ucs::language(&parse_cog::produce, &parse_cog::expect, &parse_cog::register_syntax)});
	parse_ucs::function::registry.insert({"proto", parse_ucs::language(&parse_cog::produce, &parse_cog::expect, &parse_cog::register_syntax)});
	parse_ucs::function::registry.insert({"circ", parse_ucs::language(&parse_prs::produce, &parse_prs::expect, &parse_prs::register_syntax)});

	weaver::Term::pushDialect("func", factoryCog);
	weaver::Term::pushDialect("proto", factoryCogw);
	weaver::Term::pushDialect("circ", factoryPrs);

	weaver::Project proj;
	if (proj.hasMod()) {
		proj.readMod();
	}

	proj.pushFiletype("", "wv", "", readWv, loadWv);
	proj.pushFiletype("func", "cog", "", readCog, loadCog);
	proj.pushFiletype("proto", "cogw", "", readCog, loadCogw);
	proj.pushFiletype("circ", "prs", "ckt", readPrs, loadPrs, writePrs);
	proj.pushFiletype("spice", "spi", "spi", readSpice, loadSpice, writeSpice);
	proj.pushFiletype("verilog", "v", "rtl", nullptr, nullptr, writeVerilog);
	proj.pushFiletype("layout", "gds", "gds", nullptr, loadGds, writeGds);
	proj.pushFiletype("func", "astg", "state", readAstg, loadAstg, writeAstg);
	proj.pushFiletype("proto", "astgw", "state", readAstg, loadAstgw, writeAstgw);

	vector<Proto> protos;
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
		} else if (arg == "show") {
			show = true;
		} else if (arg == "-d" or arg == "--debug") {
			debug = true;
		} else if (show) {
			protos.push_back(parseProto(proj, arg));
		}
	}

	if (not show) {
		complete();
		return is_clean();
	}

	weaver::Program prgm;
	loadGlobalTypes(prgm);

	if (protos.empty()) {
		proj.incl("top.wv");
	} else {
		for (auto i = protos.begin(); i != protos.end(); i++) {
			proj.incl(i->path);
		}
	}
	proj.load(prgm);

	if (debug) {
		prgm.print();
	} else {
		for (auto i = prgm.mods.begin(); i != prgm.mods.end(); i++) {
			for (auto j = i->terms.begin(); j != i->terms.end(); j++) {
				string name = i->name;
				if (name.rfind(proj.modName+"/", 0) == 0) {
					name = name.substr(proj.modName.size()+1);
				}
				printf("%s:", name.c_str());
				if (j->decl.recv.defined()) {
					printf("%s::", prgm.typeAt(j->decl.recv).name.c_str());
				}
				printf("%s(", j->decl.name.c_str());
				for (int k = 0; k < (int)j->decl.args.size(); k++) {
					if (k != 0) {
						printf(",");
					}
					if (j->decl.args[k].type.defined()) {
						printf("%s", prgm.typeAt(j->decl.args[k].type).name.c_str());
					}
				}
				printf(")\n");
			}
		}
	}

	complete();
	return is_clean();
}
