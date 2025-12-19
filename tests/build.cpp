#include <gtest/gtest.h>

#include "src/cli.h"

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

#include "src/weaver/builder.h"
#include "src/weaver/project.h"
#include "src/weaver/cli.h"
#include "src/format/dot.h"

#include "src/format/cog.h"
#include "src/format/spice.h"
#include "src/format/gds.h"
#include "src/format/verilog.h"
#include "src/format/prs.h"
#include "src/format/wv.h"
#include "src/format/astg.h"

#include <filesystem>

const std::filesystem::path TEST_DIR = absolute(std::filesystem::current_path() / "tests");

struct BuilderTest : ::testing::TestWithParam<std::string> {
	weaver::Project proj;
	weaver::Program prgm;                         

	BuilderTest() : proj(TEST_DIR) {
	}

	~BuilderTest() {
	}

	void SetUp() override {
		parse_ucs::function::registry.insert({"func", parse_ucs::language(&parse_cog::produce, &parse_cog::expect, &parse_cog::register_syntax)});
		parse_ucs::function::registry.insert({"proto", parse_ucs::language(&parse_cog::produce, &parse_cog::expect, &parse_cog::register_syntax)});
		parse_ucs::function::registry.insert({"circ", parse_ucs::language(&parse_prs::produce, &parse_prs::expect, &parse_prs::register_syntax)});

		weaver::Term::pushDialect("func", factoryCog);
		weaver::Term::pushDialect("proto", factoryCogw);
		weaver::Term::pushDialect("circ", factoryPrs);

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

  	loadGlobalTypes(prgm);

		/*for (auto i = prgm.mods.begin(); i != prgm.mods.end(); i++) {
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
		}*/
	}

	void TearDown() override {
	}
};

TEST_P(BuilderTest, buildProcess) {
	std::string protoStr = GetParam();
	Proto proto = parseProto(proj, protoStr);
	printf("%s\n", proto.to_string().c_str());

	proj.incl(proto.path);                        
	proj.load(prgm);

	std::vector<weaver::TermId> procs = findProto(prgm, proto);
	Build builder(proj);
	builder.testDecompose = true;
	for (size_t i = 0; i < procs.size(); i++) {
		auto term = procs[i];
		if (term.mod < 0 or term.index < 0) {
			continue;
		}
		if (prgm.mods[term.mod].terms[term.index].kind < 0) {
			fprintf(stderr, "internal:%s:%d: dialect not defined for term '%s'\n", __FILE__, __LINE__, prgm.mods[term.mod].terms[term.index].decl.name.c_str());
			return;
		}
		string dialectName = prgm.mods[term.mod].terms[term.index].dialect().name;
		if (dialectName == "func") {
			EXPECT_TRUE(builder.chpToFlow(prgm, term.mod, term.index, &procs));
		} else if (dialectName == "flow") {
			EXPECT_TRUE(builder.flowToVerilog(prgm, term.mod, term.index, &procs));
		} else if (dialectName == "proto") {
			EXPECT_TRUE(builder.hseToPrs(prgm, term.mod, term.index, &procs));
		} else if (dialectName == "circ") {
			EXPECT_TRUE(builder.prsToSpi(prgm, term.mod, term.index, &procs));
		} else if (dialectName == "spice") {
			EXPECT_TRUE(builder.spiToGds(prgm, term.mod, term.index, &procs));
		}
	}

	proj.save(prgm);
}

INSTANTIATE_TEST_SUITE_P(
	BuilderTests,
	BuilderTest,
	::testing::Values(
//		"test/stream:decimator()",
		"test/stream:zipper(chan,chan,chan)",
		"test/stream:unzipper(chan,chan,chan)",
		"test/stream:serialize_1to8(chan,chan,chan,chan,chan,chan,chan,chan,chan)",
		"test/stream:deserialize_8to1(chan,chan,chan,chan,chan,chan,chan,chan,chan)",
		"test/dsa:multi_reset(chan)",
		"test/dsa:complex_reset(chan)",
		"test/dsa:straightline(chan,chan)",
		"test/dsa:sequential_buffers(chan,chan,chan,chan,chan,chan,chan,chan)",
		"test/dsa:linear_subprograms(chan,chan,chan,chan,chan)",
//		"test/dsa:conditional_router(chan,chan,chan,chan,chan)",
		"test/cpu:imem(chan,chan)",
		"test/cpu:dmem(chan,chan,chan,chan)",
		"test/cpu:fetch(chan,chan,chan)",
		"test/cpu:decode(chan,chan,chan,chan,chan,chan,chan,chan,chan,chan,chan)",
		"test/cpu:execute(chan,chan,chan,chan)",
		"test/cpu:cpu(chan,chan,chan,chan,chan,chan)",
		"test/arithmetic:ReLU(chan,chan)",
		"test/arithmetic:sign(chan,chan)",
		"test/arithmetic:abs(chan,chan)",
		"test/arithmetic:min(chan,chan,chan)",
		"test/arithmetic:max(chan,chan,chan)",
		"test/arithmetic:median_of_three(chan,chan,chan,chan)",
		"test/arithmetic:add(chan,chan,chan)",
		"test/arithmetic:saturating_add(chan,chan,chan)",
		"test/arithmetic:multiply(chan,chan,chan)",
		"test/arithmetic:running_max(chan,chan,chan)",
		"test/arithmetic:matmul_2x2(chan,chan,chan,chan,chan,chan,chan,chan,chan,chan,chan,chan)",
		"test/projection:broadcast(chan,chan,chan,chan,chan)",
		"test/projection:pingpong(chan,chan,chan,chan)",
		"test/projection:deadlockA(chan,chan)",
		"test/projection:deadlockB(chan,chan)",
		"test/projection:deadlock()",
		"test/dataflow:source(chan)",
		"test/dataflow:sink(chan)",
		"test/dataflow:buffer(chan,chan)",
		"test/dataflow:buffer2(chan,chan)",
		"test/dataflow:seq(chan,chan,chan,chan,chan,chan)",
		"test/dataflow:par(chan,chan,chan,chan,chan,chan)",
		"test/dataflow:copy(chan,chan,chan)",
		"test/dataflow:func(chan,chan,chan)",
		"test/dataflow:split(chan,chan,chan,chan)",
		"test/dataflow:merge(chan,chan,chan,chan)",
		"test/dataflow:add(chan,chan,chan)",
		"test/dataflow:ds_add(chan,chan,chan)",
		"test/dataflow:ds_add_flat(chan,chan,chan,chan,chan,chan)",
		"test/dataflow:wchb1b()",
		"test/dataflow:test(bool,bool)",
		"test/dataflow:error_invalid()",
		"test/bench:_()",
		"test/bench:_skip()",
		"test/bench:finite()",
		"test/bench:infinite()",
		"test/algorithm:squared_distance(chan,chan,chan,chan,chan)"
	)
);
