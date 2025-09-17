#include "unpack.h"
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
			protos.push_back(parseProto(proj, arg));
		}
	}

	weaver::Program prgm;
	loadGlobalTypes(prgm);

	if (protos.empty()) {
		proj.incl("top.wv");
		proj.load(prgm);
		unpacker.unpack(prgm);
	} else {
		for (auto i = protos.begin(); i != protos.end(); i++) {
			proj.incl(i->path);
		}
		proj.load(prgm);
		for (auto i = protos.begin(); i != protos.end(); i++) {
			vector<weaver::TermId> curr = findProto(prgm, *i);
			if (curr.empty()) {
				error("", "module not found for term '" + i->to_string() + "'", __FILE__, __LINE__);
			}
			for (auto j = curr.begin(); j != curr.end(); j++) {
				unpacker.unpack(prgm, *j);
			}
		}
	}

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

