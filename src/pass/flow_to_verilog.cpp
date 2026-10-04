#include "flow_to_verilog.h"

#include <flow/synthesize.h>
#include <flow/func.h>
#include "../format/flow.h"
#include "../format/param.h"

struct FlowLinker : flow::Linker {
	weaver::Program &prgm;
	int mod;

	FlowLinker(weaver::Program &prgm, int mod) : prgm(prgm) {
		this->mod = mod;
	}
	~FlowLinker() {}

	flow::Implementation find(const flow::Instance &inst) override {
		flow::Implementation result;
		weaver::Prototype proto = protoFromInstance(prgm, inst, false);
		std::vector<weaver::TermId> terms = prgm.findTerms(proto, mod);
		if (terms.empty() or not prgm.termValid(terms[0])) {
			error("", "unable to link '" + proto.to_string() + "'", __FILE__, __LINE__);
			//prgm.print();
			return result;
		} else if (terms.size() > 1u) {
			warning("", "ambiguous instance '" + proto.to_string() + "'", __FILE__, __LINE__);
		}

		weaver::Term &term = prgm.termAt(terms[0]);
		proto = prgm.getPrototype(term.decl, prgm.mods[mod].name);

		int i = term.rfindVariant("flow");
		int j = term.rfindVariant("verilog");

		if (j < 0 or j >= (int)term.variants.size()) {
			j = term.createVariant(weaver::Variant("verilog", clocked::Module(proto.mangle(true)), i));
		}

		if (i >= 0 and i < (int)term.variants.size()) {
			result.func = &term.variants[i].as<flow::Func>();
		}

		result.mod = &term.variants[j].as<clocked::Module>();
		if (result.func != nullptr and result.mod != nullptr) {
			result.funcToMod = flow::mapToVerilog(*result.func, *result.mod);
		}
		return result;
	}
};

bool flowToVerilog(Build &builder, weaver::Program &prgm, weaver::TermId id) {
	if (builder.timing != Build::TIMING_CLOCKED
		and builder.timing != Build::TIMING_MIXED) {
		return false;
	}

	if (not id.hasVar() or prgm.varAt(id).meta.dialect != "flow") {
		return false;
	}

	FlowLinker linker(prgm, id.mod);

	flow::Func &fn = prgm.varAt(id).as<flow::Func>();
	clocked::Module rtl = flow::synthesizeModuleFromFunc(fn, &linker);

	id.var = prgm.termAt(id).createVariant(weaver::Variant("verilog", rtl, id.var));
	builder.todo.push_back(id);
	return true;
}

