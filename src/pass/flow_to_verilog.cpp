#include "flow_to_verilog.h"

#include <flow/synthesize.h>
#include <flow/func.h>

bool flowToVerilog(Build &builder, weaver::Program &prgm, weaver::TermId id) {
	if (builder.timing != Build::TIMING_CLOCKED
		and builder.timing != Build::TIMING_MIXED) {
		return false;
	}

	if (not id.hasVar() or prgm.varAt(id).meta.dialect() != "flow") {
		return false;
	}
	flow::Func &fn = prgm.varAt(id).as<flow::Func>();
	clocked::Module rtl = flow::synthesizeModuleFromFunc(fn);

	id.var = prgm.termAt(id).createVariant(weaver::Variant("verilog", rtl, id.var));
	builder.todo.push_back(id);
	return true;
}

