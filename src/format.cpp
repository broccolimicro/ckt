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
	proj.pushDialect("func", &parse_cog::factory, factoryCog, nullptr);
	proj.pushDialect("struct", &parse_gc::factory, factoryGc, nullptr);
	proj.pushDialect("proto", &parse_cog::factory, factoryCogw, nullptr);
	proj.pushDialect("circ", &parse_prs::factory, factoryPrs, nullptr);
	proj.pushDialect("spice", nullptr, nullptr, linkSpice);
	proj.pushDialect("verilog", nullptr, nullptr, nullptr);
	proj.pushDialect("layout", nullptr, nullptr, nullptr);
	proj.pushDialect("func", nullptr, nullptr, nullptr);
	proj.pushDialect("proto", nullptr, nullptr, nullptr);

	proj.pushFiletype("", "wv", readWv, loadWv, nullptr, weaver::Filetype::MODULE);
	proj.pushFiletype("func", "cog", readCog, loadCog);
	proj.pushFiletype("struct", "gc", readGc, loadGc, writeGc);
	proj.pushFiletype("proto", "cogw", readCog, loadCogw);
	proj.pushFiletype("circ", "prs", readPrs, loadPrs, writePrs);
	proj.pushFiletype("spice", "spi", readSpice, loadSpice, writeSpice, weaver::Filetype::MODULE);
	proj.pushFiletype("verilog", "v", nullptr, nullptr, writeVerilog);
	proj.pushFiletype("layout", "gds", nullptr, loadGds, writeGds, weaver::Filetype::PROJECT);
	proj.pushFiletype("func", "astg", readAstg, loadAstg, writeAstg);
	proj.pushFiletype("proto", "astgw", readAstg, loadAstgw, writeAstgw);
}
