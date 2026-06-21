#include "format.h"

#include <parse/parse.h>
#include <parse/default/block_comment.h>
#include <parse/default/line_comment.h>
#include <parse/default/new_line.h>

#include <parse_ucs/source.h>
#include <parse_astg/factory.h>
#include <parse_cog/factory.h>
#include <parse_gc/factory.h>
#include <parse_chp/factory.h>
#include <parse_prs/factory.h>
#include <parse_spice/factory.h>

#include <interpret_wv/import.h>

#include "format/cog.h"
#include "format/gc.h"
#include "format/spice.h"
#include "format/gds.h"
#include "format/verilog.h"
#include "format/prs.h"
#include "format/wv.h"
#include "format/astg.h"

void loadAllFormats(weaver::Project &proj) {
	parse_ucs::function::registry.insert({"func", parse_ucs::language(&parse_cog::produce, &parse_cog::expect, &parse_cog::register_syntax)});
	parse_ucs::function::registry.insert({"struct", parse_ucs::language(&parse_gc::produce, &parse_gc::expect, &parse_gc::register_syntax)});
	parse_ucs::function::registry.insert({"proto", parse_ucs::language(&parse_cog::produce, &parse_cog::expect, &parse_cog::register_syntax)});
	parse_ucs::function::registry.insert({"circ", parse_ucs::language(&parse_prs::produce, &parse_prs::expect, &parse_prs::register_syntax)});

	weaver::Language lang;
	lang.dialects.insert({"func", factoryCog});
	lang.dialects.insert({"struct", factoryGc});
	lang.dialects.insert({"proto", factoryCogw});
	lang.dialects.insert({"circ", factoryPrs});

	proj.pushFiletype("", "wv", "", readWv, loadWv, nullptr, weaver::Filetype::MODULE, lang);
	proj.pushFiletype("func", "cog", "", readCog, loadCog);
	proj.pushFiletype("struct", "gc", "", readGc, loadGc, writeGc);
	proj.pushFiletype("proto", "cogw", "", readCog, loadCogw);
	proj.pushFiletype("circ", "prs", "ckt", readPrs, loadPrs, writePrs);
	proj.pushFiletype("spice", "spi", "spi", readSpice, loadSpice, writeSpice, weaver::Filetype::MODULE);
	proj.pushFiletype("verilog", "v", "rtl", nullptr, nullptr, writeVerilog);
	proj.pushFiletype("layout", "gds", "gds", nullptr, loadGds, writeGds, weaver::Filetype::PROJECT);
	proj.pushFiletype("func", "astg", "state", readAstg, loadAstg, writeAstg);
	proj.pushFiletype("proto", "astgw", "state", readAstg, loadAstgw, writeAstgw);
}
