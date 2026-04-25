#include "prs_to_spi.h"

#include "../back/asic.h"

#include <prs/bubble.h>
#include <prs/synthesize.h>
#include <sch/Netlist.h>
#include <phy/Script.h>

bool bubble(const Build &builder, weaver::Program &prgm, weaver::TermId id) {
	if (not id.hasVar() or prgm.varAt(id).meta.dialect != "circ") {
		return false;
	}
	if (prgm.varAt(id).meta.has("circ.bubble")) {
		return true;
	}

	prs::production_rule_set &pr = prgm.varAt(id).as<prs::production_rule_set>();

	prs::bubble bub;
	bub.load_prs(pr);
	for (auto i = bub.net.begin(); i != bub.net.end(); i++) {
		bub.step(i);
		//auto result = bub.step(i);
		//if (has(Build::BUBBLE) and debug and result.second) {
		//	gvdot::render(pr.name+"_bubble" + to_string(++step) + ".png", export_bubble(bub, pr).to_string());
		//}
	}
	bub.complete();
	bub.save_prs(&pr);
 
	prgm.varAt(id).meta.set("circ.bubble");
	prgm.varAt(id).meta.unset("circ.sizing");
	return true;
}

bool keepers(const Build &builder, weaver::Program &prgm, weaver::TermId id) {
	if (not id.hasVar() or prgm.varAt(id).meta.dialect != "circ") {
		return false;
	}
	if (prgm.varAt(id).meta.has("circ.keepers")) {
		return true;
	}

	prs::production_rule_set &pr = prgm.varAt(id).as<prs::production_rule_set>();

	pr.add_keepers(true, false, 1, builder.progress);
 
	prgm.varAt(id).meta.set("circ.keepers");
	prgm.varAt(id).meta.unset("circ.sizing");
	return true;
}

bool sizing(const Build &builder, weaver::Program &prgm, weaver::TermId id) {
	if (not id.hasVar() or prgm.varAt(id).meta.dialect != "circ") {
		return false;
	}
	if (prgm.varAt(id).meta.has("circ.sizing")) {
		return true;
	}

	prs::production_rule_set &pr = prgm.varAt(id).as<prs::production_rule_set>();
	pr.size_devices(0.1, builder.progress);

	prgm.varAt(id).meta.set("circ.sizing");
	return true;
}

bool prsToSpi(Build &builder, weaver::Program &prgm, weaver::TermId id) {
	if (not id.hasVar() or prgm.varAt(id).meta.dialect != "circ"
		or not prgm.varAt(id).meta.has("circ.keepers")
		or not prgm.varAt(id).meta.has("circ.sizing")) {
		return false;
	}

	phy::Tech *tech = loadASIC(builder.proj);
	if (not tech) {
		return false;
	}

	prs::production_rule_set &pr = prgm.varAt(id).as<prs::production_rule_set>();

	sch::Netlist *net = prgm.getLib<sch::Netlist>("spice");
	int index = (int)net->subckts.size();
	net->subckts.push_back(prs::build_netlist(*tech, pr, builder.progress));
	if (builder.debug) {
		net->subckts.back().print();
	}

	id.var = prgm.termAt(id).createVariant(weaver::Variant("spice", index, id.var));
	builder.todo.push_back(id);
	return true;
}

