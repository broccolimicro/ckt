#include <gtest/gtest.h>

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

#include <weaver/project.h>

#include "src/weaver/builder.h"

#include "src/format.h"

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
		loadAllFormats(proj);

		if (proj.hasMod()) {
			readMod(proj);
		}

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
	weaver::Prototype proto(protoStr);
	printf("%s\n", proto.to_string().c_str());

	proj.incl(proto.mod);                        
	proj.load(prgm);

	std::vector<weaver::TermId> procs = prgm.findTerms(proto);
	Build builder(proj);
	builder.testDecompose = true;
	builder.build(prgm);
	proj.save(prgm);
}

INSTANTIATE_TEST_SUITE_P(
	BuilderTests,
	BuilderTest,
	::testing::Values(
		"test.multi_reset(chan)",
		"test.complex_reset(chan)",
		"test.straightline(chan,chan)",
		"test.sequential_buffers(chan,chan,chan,chan,chan,chan,chan,chan)",
		"test.linear_subprograms(chan,chan,chan,chan,chan)",
		"test.conditional_router(chan,chan,chan,chan,chan)",
		"test.squared_distance(chan,chan,chan,chan,chan)",
		"test.source(chan)",
		"test.sink(chan)",
		"test.buffer(chan,chan)",
		"test.buffer2(chan,chan)",
		"test.seq(chan,chan,chan,chan,chan,chan)",
		"test.par(chan,chan,chan,chan,chan,chan)",
		"test.copy(chan,chan,chan)",
		"test.func(chan,chan,chan)",
		"test.split(chan,chan,chan,chan)",
		"test.merge(chan,chan,chan,chan)",
		"test.add(chan,chan,chan)",
		"test.ds_add(chan,chan,chan)",
		"test.ds_add_flat(chan,chan,chan,chan,chan,chan)",
		"test.wchb1b()",
		"test.test(bool,bool)",
		"test.error_invalid()",
		"test.ermputs(chan,chan,chan)",
		"test.ragistar(chan,chan)",
		"test.beth_screen()",
		"test.together_forever(chan,chan,chan,chan)",
		"test.operating_table()",
		"test.broadcast(chan,chan,chan,chan,chan)",
		"test.pingpong(chan,chan,chan,chan)",
		"test.deadlockA(chan,chan)",
		"test.deadlockB(chan,chan)",
		"test.deadlock()",
		"test.decimator()",
		"test.zipper(chan,chan,chan)",
		"test.unzipper(chan,chan,chan)",
		"test.serialize_1to8(chan,chan,chan,chan,chan,chan,chan,chan,chan)",
		"test.deserialize_8to1(chan,chan,chan,chan,chan,chan,chan,chan,chan)",
		"test.ReLU(chan,chan)",
		"test.sign(chan,chan)",
		"test.abs(chan,chan)",
		"test.min(chan,chan,chan)",
		"test.max(chan,chan,chan)",
		"test.median_of_three(chan,chan,chan,chan)",
		"test.add(chan,chan,chan)",
		"test.saturating_add(chan,chan,chan)",
		"test.multiply(chan,chan,chan)",
		"test.running_max(chan,chan,chan)",
		"test.matmul_2x2(chan,chan,chan,chan,chan,chan,chan,chan,chan,chan,chan,chan)",
		"test._()",
		"test._skip()",
		"test.finite()",
		"test.infinite()",
		"test.imem(chan,chan)",
		"test.dmem(chan,chan,chan,chan)",
		"test.fetch(chan,chan,chan)",
		"test.decode(chan,chan,chan,chan,chan,chan,chan,chan,chan,chan,chan)",
		"test.execute(chan,chan,chan,chan)",
		"test.cpu(chan,chan,chan,chan,chan,chan)"
	)
);
