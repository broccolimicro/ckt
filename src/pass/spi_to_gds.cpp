#include "spi_to_gds.h"

#include "../back/asic.h"
#include "../format/cell.h"

#include <sch/Subckt.h>
#include <sch/Tapeout.h>
#include <sch/Placer.h>

weaver::Decl declFromSubckt(const weaver::Program &prgm, const sch::Subckt &ckt) {
	weaver::TypeId wireType(prgm.global, prgm.mods[prgm.global].findType("wire"));

	weaver::Decl decl;
	decl.name = ckt.name;

	// All of the ports in a cell are wires
	vector<weaver::Instance> args;
	for (int j : ckt.ports) {
		decl.args.push_back(weaver::Instance(wireType, ckt.nets[j].name));
	}
	return decl;
}

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
	builder.todo.push_back(id);

	// Load the cells into weaver as terms
	// Loop through all subckt instantiations and make sure they have a term in
	// the weaver program, and then add them to the todo list for the builder.
	for (auto &cell : cells) {
		std::string originalName = cell.name;
		std::string baseName = "cell_" + encodeBase32(cell.id);

		weaver::Decl decl = declFromSubckt(prgm, cell);
		decl.name = baseName;
		int mod = prgm.getModule(builder.proj.tech.name);

		// Create the term and schedule it for compilation
		int step = 0;
		weaver::TermId cellId;
		while (true) {
			cellId = prgm.getTerm(mod, decl);
			cell.name = prgm.getPrototype(cellId).mangle();
			if (prgm.termAt(cellId).variants.empty()) {
				cellId.var = prgm.termAt(cellId).createVariant(weaver::Variant("spice", cell));
				prgm.varAt(cellId).meta.set("spi.cell");
				builder.todo.push_back(cellId);
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

		prgm.varAt(id).as<sch::Subckt>().renameType(originalName, cell.name);
	}
	return true;
}

struct SchLinker : sch::Linker {
	const weaver::Program &prgm;

	SchLinker(const weaver::Program &prgm) : prgm(prgm) {}
	~SchLinker() {}

	sch::Implementation find(const sch::Instance &inst) override {
		sch::Implementation result;

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
	if (not ckt.mos.empty() and not ckt.inst.empty()) {
		error("", "no support for mixed macro/micro layout", __FILE__, __LINE__);
		return false;
	}

	if (ckt.inst.empty()) {
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
		builder.todo.push_back(id);
		return true;
	} else if (ckt.mos.empty()) {
		/*weaver::TermId placedId = id;
		placedId.var = prgm.termAt(id).createVariant(weaver::Variant("layout", phy::Layout(*tech), id.var));

		phy::Layout &macro = prgm.varAt(placedId).as<phy::Layout>();

		SchLinker linker(prgm);
		sch::Placer placer(&linker, 0, 0, builder.progress, builder.debug);
		placer.load(sch::Implementation(&ckt, &macro));
		sch::Placement prob(placer, 0);
		prob.solve();
		prob.save(macro);*/
		return true;
	}
	return false;
}

/*void doPlacement(phy::Library &lib, sch::Netlist &lst, gdstk::GdsWriter *stream=nullptr, map<int, gdstk::Cell*> *cells=nullptr, bool progress=false, bool debug=false) {
	if (progress) {
		printf("Placing Cells:\n");
	}

	if (lib.macros.size() < lst.subckts.size()) {
		lib.macros.resize(lst.subckts.size(), Layout(*lib.tech));
	}

	sch::Placer placer(lib, lst, 0, 0, progress, debug);

	Timer total;
	for (int i = 0; i < (int)lst.subckts.size(); i++) {
		if (not lst.subckts[i].isCell) {
			if (progress) {
				printf("  %s...", lst.subckts[i].name.c_str());
				fflush(stdout);
			}
			Timer tmr;
			lib.macros[i].name = lst.subckts[i].name;
			placer.place(i);
			if (progress) {
				int area = 0;
				for (auto j = lst.subckts[i].inst.begin(); j != lst.subckts[i].inst.end(); j++) {
					if (lst.subckts[j->subckt].isCell) {
						area += lib.macros[j->subckt].box.area();
					}
				}
				printf("[%s%d DBUNIT2 AREA%s]\t%gs\n", KGRN, area, KNRM, tmr.since());
			}
			if (stream != nullptr and cells != nullptr) {
				export_layout(*stream, lib, i, *cells);
			}
			lst.mapToLayout(i, lib.macros[i]);
		}
	}

	if (progress) {
		printf("done\t%gs\n\n", total.since());
	}
}*/

