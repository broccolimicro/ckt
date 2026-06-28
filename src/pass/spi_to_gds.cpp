#include "spi_to_gds.h"

#include "../back/asic.h"
#include "../format/cell.h"
#include "../format/spice.h"

#include <sch/Subckt.h>
#include <sch/Tapeout.h>
#include <sch/Placer.h>

#include <weaver/params.h>
#include <interpret_wv/export.h>
#include <common/timer.h>

bool mapCells(Build &builder, weaver::Program &prgm, weaver::TermId id) {
	if (not id.hasVar() or not builder.get(Build::MAP)
		or prgm.varAt(id).meta.dialect != "spice") {
		return false;
	}
	if (prgm.varAt(id).meta.has("spi.mapped") or prgm.varAt(id).meta.has("spi.cell")) {
		return true;
	}
	phy::Tech *tech = loadASIC(builder.proj);
	if (not tech) {
		return false;
	}

	sch::Subckt ckt = prgm.varAt(id).as<sch::Subckt>();
	if (ckt.isCell) {
		return true;
	}

	vector<sch::Subckt> cells = sch::mapCells(*tech, ckt, builder.progress);
	id.var = prgm.termAt(id).createVariant(weaver::Variant("spice", ckt, id.var));
	prgm.varAt(id).meta.set("spi.mapped");
	builder.push(prgm, id);

	// Load the cells into weaver as terms
	// Loop through all subckt instantiations and make sure they have a term in
	// the weaver program, and then add them to the todo list for the builder.
	for (auto &cell : cells) {
		std::string originalName = cell.name;
		std::string baseName = "cell_" + encodeBase32(cell.id);

		int mod = prgm.getModule(builder.proj.tech.name);
		weaver::Decl decl = declFromSubckt(prgm, mod, cell);
		decl.name = baseName;
		cell.comment = weaver::writeParams({{"decl", weaver::export_decl(prgm, decl).to_string()}});

		// Create the term and schedule it for compilation
		int step = 0;
		weaver::TermId cellId;
		while (true) {
			cellId = prgm.getTerm(mod, decl);
			cell.name = prgm.getPrototype(cellId).mangle();
			if (prgm.termAt(cellId).variants.empty()) {
				cellId.var = prgm.termAt(cellId).createVariant(weaver::Variant("spice", cell));
				prgm.varAt(cellId).meta.set("spi.cell");
				builder.push(prgm, cellId);
				break;
			}

			cellId.var = prgm.termAt(cellId).rfindVariant("spice");
			if (cellId.var >= 0 and cell.compare(prgm.varAt(cellId).as<sch::Subckt>()) == 0) {
				// This cell is already defined in the library
				break;
			}

			// There is a conflicting cell already in the library with this name
			decl.name = baseName + "_" + ::to_string(++step);
		}

		sch::Subckt &mapped = prgm.varAt(id).as<sch::Subckt>();
		for (auto i = mapped.inst.begin(); i != mapped.inst.end(); i++) {
			if (i->type == originalName) {
				i->type = cell.name;
				i->comment = weaver::writeParams({{"proto", prgm.getPrototype(cellId).to_string()}});
			}
		}
	}
	return true;
}

struct SchLinker : sch::Linker {
	const phy::Tech &tech;
	weaver::Program &prgm;
	int mod;

	SchLinker(const phy::Tech &tech, weaver::Program &prgm, int mod) : tech(tech), prgm(prgm) {
		this->mod = mod;
	}
	~SchLinker() {}

	sch::Implementation find(const sch::Instance &inst) override {
		sch::Implementation result;
		weaver::Prototype proto = protoFromInstance(prgm, mod, inst, false);
		std::vector<weaver::TermId> terms = prgm.findTerms(proto, mod);
		if (terms.empty() or not prgm.termValid(terms[0])) {
			error("", "unable to link '" + proto.to_string() + "'", __FILE__, __LINE__);
			return result;
		} else if (terms.size() > 1u) {
			warning("", "ambiguous instance '" + proto.to_string() + "'", __FILE__, __LINE__);
		}

		weaver::Term &term = prgm.termAt(terms[0]);
		proto = prgm.getPrototype(term.decl, prgm.mods[mod].name);

		int i = term.rfindVariant("spice");
		if (i >= 0 and i < (int)term.variants.size()) {
			result.ckt = &term.variants[i].as<sch::Subckt>();
		}

		int j = term.rfindVariant("layout");
		if (j < 0 or j >= (int)term.variants.size()) {
			j = term.createVariant(weaver::Variant("layout", phy::Layout(tech, proto.mangle(true)), i));
		}

		result.macro = &term.variants[j].as<phy::Layout>();
		if (result.ckt != nullptr and result.macro != nullptr) {
			result.cktToMacro = result.ckt->mapToLayout(*result.macro);
		}
		return result;
	}
};

bool spiToGds(Build &builder, weaver::Program &prgm, weaver::TermId id) {
	if (not id.hasVar()
		or prgm.varAt(id).meta.dialect != "spice") {
		printf("not spice\n");
		return false;
	}
	phy::Tech *tech = loadASIC(builder.proj);
	if (not tech) {
		printf("tech failed to load\n");
		return false;
	}

	sch::Subckt ckt = prgm.varAt(id).as<sch::Subckt>();
	if (ckt.inst.empty() and not ckt.mos.empty()) {
		array<int, 2> vars{-1, -1};
		if (not cell::import_cell(builder.proj.tech.lib, *tech, prgm.termAt(id), &vars, builder.progress, builder.debug)) {
			// We generated a new cell, save this to the cell library
			if (not filesystem::exists(builder.proj.tech.lib)) {
				filesystem::create_directory(builder.proj.tech.lib);
			}
			cell::export_cell(builder.proj.tech.lib, *tech, prgm.termAt(id));
		}

		if (vars[1] < 0) {
			return false;
		}

		id.var = vars[1];
		prgm.varAt(id).meta.set("gds.cell");
		builder.push(prgm, id);
		return true;
	} else if (ckt.mos.empty() and not ckt.inst.empty()) {
		if (builder.progress) {
			printf("Placing %s...", ckt.name.c_str());
			fflush(stdout);
		}
		Timer tmr;
		weaver::Term &term = prgm.termAt(id);
		weaver::Prototype proto = prgm.getPrototype(term.decl, prgm.mods[id.mod].name);

		id.var = term.createVariant(weaver::Variant("layout", phy::Layout(*tech, proto.mangle(true)), id.var));
		phy::Layout &macro = term.variants[id.var].as<phy::Layout>();

		SchLinker linker(*tech, prgm, id.mod);
		sch::Placer placer(&linker, 0, 0, builder.progress, builder.debug);
		placer.load(sch::Implementation(&ckt, &macro));
		sch::Placement prob(placer, 0);
		prob.solve();
		prob.save(macro);
		prgm.varAt(id).meta.set("gds.place");
		builder.push(prgm, id);
		if (builder.progress) {
			printf("[%sDONE%s]\t%gs\n", KGRN, KNRM, tmr.since());
		}
		return true;
	} else if (ckt.mos.empty() and ckt.inst.empty()) {
		weaver::Term &term = prgm.termAt(id);
		weaver::Prototype proto = prgm.getPrototype(term.decl, prgm.mods[id.mod].name);
		warning("", "found blackbox \"" + proto.to_string() + "\"", __FILE__, __LINE__);

		id.var = term.createVariant(weaver::Variant("layout", phy::Layout(*tech, proto.mangle(true)), id.var));
		return true;
	} else {
		weaver::Term &term = prgm.termAt(id);
		weaver::Prototype proto = prgm.getPrototype(term.decl, prgm.mods[id.mod].name);
		error("", "no support for mixed macro/micro layout in \"" + proto.to_string() + "\"", __FILE__, __LINE__);
		return false;
	}
	return false;
}
