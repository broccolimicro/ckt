#include "cell.h"

#include <common/timer.h>
#include <sch/Tapeout.h>

#include <filesystem>

using namespace std::filesystem;

namespace cell {

void export_cell(std::string path, const phy::Tech &tech, std::string mod, const weaver::Term &term) {
	bool spiceFound = false;
	bool layoutFound = false;
	string cellPath = path + "/" + term.decl.name;
	// TODO(edward.bingham) this assumes that the appropriate subckt and layout
	// to export is the most recently compiled variant. However, this may not
	// be the case, we need to have more logic about the compile graph here.
	for (auto i = term.variants.rbegin(); i != term.variants.rend(); i++) {
		if (not spiceFound and i->meta.dialect == "spice") {
			export_spi(cellPath+".spi", tech, i->as<sch::Subckt>());
			spiceFound = true;
		} else if (not layoutFound and i->meta.dialect == "layout") {
			export_layout(cellPath+".gds", i->as<phy::Layout>());
			export_lef(cellPath+".lef", i->as<phy::Layout>());
			layoutFound = true;
		}
	}
}

void export_cells(std::string path, const phy::Tech &tech, const weaver::Module &mod) {
	if (not filesystem::exists(path)) {
		filesystem::create_directory(path);
	}
	for (auto i = mod.terms.begin(); i != mod.terms.end(); i++) {
		if (i->name.rfind("cell_", 0) == 0) {
			export_cell(path, tech, mod.name, *i);
		}
	}
}

void export_cells(std::string path, const phy::Tech &tech, const weaver::Program &prgm) {
	if (not filesystem::exists(path)) {
		filesystem::create_directory(path);
	}
	for (auto i = prgm.mods.begin(); i != prgm.mods.end(); i++) {
		export_cells(path, tech, *i);
	}
}

// returns whether the cell was imported
bool import_cell(std::string path, weaver::Term &term, bool progress, bool debug) {
	string cellPath = path + "/" + term.decl.name+".gds";
	if (progress) {
		printf("  %s...", term.decl.name.c_str());
		fflush(stdout);
		printf("[");
	}

	Timer tmr;
	float searchDelay = 0.0;
	float genDelay = 0.0;

	int spiIdx = -1;
	int phyIdx = -1;

	for (int i = (int)term.variants.size()-1; i >= 0; i--) {
		if (spiIdx < 0 and term.variants[i].meta.dialect == "spice") {
			spiIdx = i;
		} else if (phyIdx < 0 and term.variants[i].meta.dialect == "layout") {
			phyIdx = i;
		}
	}

	if (spiIdx < 0) {
		// TODO(edward.bingham) should we try to load the spi in the cell library then?
		return false;
	}

	if (phyIdx >= 0) {
		// TODO(edward.bingham) check for updates?
		return true;
	}

	sch::Subckt spiNet = term.variants[spiIdx].as<sch::Subckt>();
	spiNet.cleanDangling(true);
	spiNet.combineDevices();
	spiNet.canonicalize();

	if (filesystem::exists(cellPath)) {
		phy::Layout macro;
		bool imported = import_layout(macro, cellPath, spiNet.name);
		if (progress) {
			if (imported) {
				macro.trace();
				sch::Subckt gdsNet(true);
				extract(gdsNet, macro, true);
				gdsNet.cleanDangling(true);
				gdsNet.combineDevices();
				gdsNet.canonicalize();
				searchDelay = tmr.since();
				if (gdsNet.compare(spiNet) == 0) {
					printf("%sFOUND %d DBUNIT2 AREA%s]\t%gs\n", KGRN, macro.box.area(), KNRM, searchDelay);
				} else {
					printf("%sFAILED LVS%s, ", KRED, KNRM);
					imported = false;
				}
			} else {
				searchDelay = tmr.since();
				printf("%sFAILED IMPORT%s, ", KRED, KNRM);
			}
		}
		if (imported) {
			phyIdx = term.createVariant(weaver::Variant("layout", macro));
			return true;
		} else {
			macro.clear();
		}
	}

	tmr.reset();

	int result = sch::buildCell(macro, spiNet);
	if (progress) {
		if (result == 1) {
			genDelay = tmr.since();
			printf("%sFAILED PLACEMENT%s]\t(%gs %gs)\n", KRED, KNRM, searchDelay, genDelay);
		} else if (result == 2) {
			genDelay = tmr.since();
			printf("%sFAILED ROUTING%s]\t(%gs %gs)\n", KRED, KNRM, searchDelay, genDelay);
		} else {
			sch::Subckt gdsNet(true);
			extract(gdsNet, macro, true);
			gdsNet.cleanDangling(true);
			gdsNet.combineDevices();
			gdsNet.canonicalize();

			genDelay = tmr.since();
			if (gdsNet.compare(spiNet) == 0) {
				printf("%sGENERATED %d DBUNIT2 AREA%s]\t(%gs %gs)\n", KGRN, macro.box.area(), KNRM, searchDelay, genDelay);
				phyIdx = term.createVariant(weaver::Variant("layout", macro));
			} else {
				printf("%sFAILED LVS%s]\t(%gs %gs)\n", KRED, KNRM, searchDelay, genDelay);
				if (debug) {
					gdsNet.print();
					spiNet.print();
				}
			}
		}
	}
	return false;
}

void update_library(std::string path, weaver::Program &prgm, gdstk::GdsWriter *stream, map<int, gdstk::Cell*> *cells, bool progress, bool debug) {
	bool libFound = filesystem::exists(path);
	if (progress) {
		printf("Load cell layouts:\n");
	}

	Timer tmr;
	for (int i = 0; i < (int)lst.subckts.size(); i++) {
		if (lst.subckts[i].isCell) {
			if (not import_cell(path, lib, lst, i, progress, debug)) {
				// We generated a new cell, save this to the cell library
				if (not libFound) {
					filesystem::create_directory(path);
					libFound = true;
				}
				string cellPath = path + "/" + lib.macros[i].name;
				export_layout(cellPath+".gds", lib.macros[i]);
				export_lef(cellPath+".lef", lib.macros[i]);
				export_spi(cellPath+".spi", *lib.tech, lst, lst.subckts[i]);
			}
			if (stream != nullptr and cells != nullptr) {
				export_layout(*stream, lib, i, *cells);
			}
			lst.mapToLayout(i, lib.macros[i]);
		}
	}
	if (progress) {
		printf("done\t%gs\n\n", tmr.since());
	}
}

}
