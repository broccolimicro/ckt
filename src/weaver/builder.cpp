#include "builder.h"

#include <filesystem>

#include <common/standard.h>
#include <common/timer.h>
#include <common/text.h>

#include <chp/synthesize.h>
#include <flow/synthesize.h>

#include <hse/elaborator.h>
#include <hse/encoder.h>
#include <hse/synthesize.h>
#include <prs/bubble.h>
#include <prs/synthesize.h>
#include <sch/Netlist.h>
#include <sch/Tapeout.h>
#include <sch/Placer.h>
#include <phy/Script.h>

#include <interpret_chp/export_dot.h>
#include <interpret_flow/export_dot.h>
#include <interpret_flow/export_verilog.h>
#include <interpret_hse/export_cli.h>
#include <interpret_hse/export.h>
#include <interpret_phy/import.h>
#include <interpret_phy/export.h>
#include <interpret_prs/export.h>
#include <interpret_sch/export.h>

#include "../format/cell.h"
#include "../format/dot.h"

#define MAX_PROCESS_SIZE 256

Build::Build(weaver::Project &proj) : proj(proj) {
	logic = LOGIC_CMOS;
	timing = TIMING_MIXED;
	stage = -1;

	doPreprocess = false;
	doPostprocess = false;

	testDecompose = false;

	noCells = false;
	noGhosts = false;

	progress = false;
	debug = false;
	format_expressions_as_html_table = false;
	
	targets.resize(ROUTE+1, false);
}

Build::~Build() {
}

void Build::set(int target) {
	stage = stage < target ? target : stage;
	targets[target] = true;
}

bool Build::get(int target) const {
	return stage < 0 or stage >= target;
}

void Build::inclAll() {
	targets = vector<bool>(ROUTE+1, true);
}

void Build::incl(int target) {
	targets[target] = true;
}

void Build::excl(int target) {
	targets[target] = false;
}

bool Build::has(int target) const {
	return targets[target];
}

