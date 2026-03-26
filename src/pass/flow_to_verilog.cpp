#include "flow_to_verilog.h"

#include <flow/synthesize.h>
#include <flow/func.h>

bool flowToVerilog(Build &builder, weaver::Program &prgm, weaver::TermId &id, int index) {
	if (builder.timing != Build::TIMING_CLOCKED
		and builder.timing != Build::TIMING_MIXED) {
		return false;
	}

	weaver::Term &term = prgm.termAt(id);
	weaver::Variant &var = term.variants[index];
	if (var.meta.dialect() != "flow") {
		return false;
	}
	flow::Func &fn = var.as<flow::Func>();

	var.derived.push_back(term.variants.size());
	term.variants.push_back(
		weaver::Variant(
			index,
			flow::synthesizeModuleFromFunc(fn),
			weaver::Metadata(weaver::Term::findDialect("verilog"))));
	return true;
}

