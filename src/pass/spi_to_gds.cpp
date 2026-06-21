#include "spi_to_gds.h"

#include "../back/asic.h"
#include "../format/cell.h"

#include <sch/Subckt.h>
#include <sch/Tapeout.h>

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

bool buildCell(Build &builder, weaver::Program &prgm, weaver::TermId id) {
	if (not id.hasVar()
		or prgm.varAt(id).meta.dialect != "spice") {
		printf("not spice\n");
		return false;
	}
	if (prgm.varAt(id).meta.has("spi.mapped")) {
		return true;
	}
	if (not prgm.varAt(id).meta.has("spi.cell")) {
		printf("no spi.cell\n");
		return false;
	}
	phy::Tech *tech = loadASIC(builder.proj);
	if (not tech) {
		printf("tech failed to load\n");
		return false;
	}

	array<int, 2> vars{-1, -1};
	if (not cell::import_cell(builder.proj.tech.lib, *tech, prgm.termAt(id), &vars, builder.progress, builder.debug)) {
		// We generated a new cell, save this to the cell library
		if (not filesystem::exists(builder.proj.tech.lib)) {
			filesystem::create_directory(builder.proj.tech.lib);
		}
		cell::export_cell(builder.proj.tech.lib, *tech, prgm.termAt(id));
	}

	if (vars[1] >= 0) {
		id.var = vars[1];
		builder.todo.push_back(id);
		return true;
	}
	printf("layout not defined after import\n");
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
}

bool spiToGds(const Build &builder, weaver::Program &prgm, weaver::TermId id) {
	if (not id.hasVar() or
		or prgm.varAt(id).meta.dialect() != "spice") {
		return false;
	}
	if (prgm.varAt(id).meta.has("spi.cells")) {
		return true;
	}
	phy::Tech *tech = loadASIC(builder);
	if (not tech) {
		return false;
	}

	phy::Library lib(proj.tech);
	map<int, gdstk::Cell*> cells;
	if (builder.get(Build::CELLS)) {
		cell::update_library(lib, net, nullptr, &cells, progress, debug);
	}

	if (get(Build::PLACE)) {
		doPlacement(lib, net, nullptr, &cells, progress, debug);
	}
}

bool Build::spiToGds(weaver::Program &prgm, int modIdx, int termIdx, vector<weaver::TermId> *result) {
	std::filesystem::path debugDirPath = proj.rootDir / proj.BUILD / "dbg";
	string debugDir = debugDirPath.string();

	// Verify expected format of the term
	if (term.dialect().name != "spice") {
		fprintf(stderr, "error: dialect '%s' not supported for translation from spice to gds.\n",
			term.dialect().name.c_str());
		return false;
	}

	// Create dialect and module
	int gdsKind = weaver::Term::getDialect("layout");
	int gdsIdx = prgm.getModule(prgm.mods[modIdx].name + ">>layout");

	const weaver::Decl &decl = term.decl;
	if (decl.ret.defined() or decl.recv.defined()) {
		fprintf(stderr, "error: spice must be a full process for synthesis\n");
		return false;
	}

	// Create the new term in the module
	string name = decl.name;
	vector<weaver::Instance> args = decl.args;

	sch::Netlist &net = term.as<sch::Netlist>();

	if (noCells) {
		for (int i = 0; i < (int)net.subckts.size(); i++) {
			net.subckts[i].isCell = true;
		}
	}

	if (not proj.tech.isLoaded() and not phy::loadTech(proj.tech)) {
		cout << "Unable to load techfile \'" + proj.tech.path + "\'." << endl;
		return false;
	}

	Timer cellsTmr;
	if (get(Build::MAP)) {
		if (progress) printf("Break subckts into cells:\n");
		net.mapCells(proj.tech, progress);
		if (progress) printf("done\t%gs\n\n", cellsTmr.since());
	}

	phy::Library lib(proj.tech);
	map<int, gdstk::Cell*> cells;
	if (get(Build::CELLS)) {
		cell::update_library(lib, net, nullptr, &cells, progress, debug);
	}

	if (get(Build::PLACE)) {
		doPlacement(lib, net, nullptr, &cells, progress, debug);
	}

	int dstIdx = prgm.mods[gdsIdx].createTerm(weaver::Term::procOf(gdsKind, name, args));
	if (result != nullptr) {
		result->push_back(weaver::TermId(gdsIdx, dstIdx));
	}
	prgm.mods[gdsIdx].terms[dstIdx].def = lib;
	return true;
}*/

