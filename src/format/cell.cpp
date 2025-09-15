#include "cell.h"

#include <common/timer.h>
#include <sch/Tapeout.h>

#include <filesystem>

using namespace std::filesystem;

namespace cell {

void export_cell(int index, const phy::Library &lib, const sch::Netlist &net) {
	if (lib.macros[index].name.rfind("cell_", 0) == 0) {
		string cellPath = lib.tech->lib + "/" + lib.macros[index].name;
		if (not filesystem::exists(cellPath+".gds")) {
			export_layout(cellPath+".gds", lib.macros[index]);
			export_lef(cellPath+".lef", lib.macros[index]);
			if (index < (int)net.subckts.size()) {
				export_spi(cellPath+".spi", *lib.tech, net, net.subckts[index]);
			}
		}
	}
}

void export_cells(const phy::Library &lib, const sch::Netlist &net) {
	if (not filesystem::exists(lib.tech->lib)) {
		filesystem::create_directory(lib.tech->lib);
	}
	for (int i = 0; i < (int)lib.macros.size(); i++) {
		export_cell(i, lib, net);
	}
}

// returns whether the cell was imported
bool import_cell(phy::Library &lib, sch::Netlist &lst, int idx, bool progress, bool debug) {
	if (idx >= (int)lib.macros.size()) {
		lib.macros.resize(idx+1, Layout(*lib.tech));
	}
	lib.macros[idx].name = lst.subckts[idx].name;
	string cellPath = lib.tech->lib + "/" + lib.macros[idx].name+".gds";
	if (progress) {
		printf("  %s...", lib.macros[idx].name.c_str());
		fflush(stdout);
		printf("[");
	}

	Timer tmr;
	float searchDelay = 0.0;
	float genDelay = 0.0;

	sch::Subckt spiNet = lst.subckts[idx];
	spiNet.cleanDangling(true);
	spiNet.combineDevices();
	spiNet.canonicalize();

	if (filesystem::exists(cellPath)) {
		bool imported = import_layout(lib.macros[idx], cellPath, lib.macros[idx].name);
		if (progress) {
			if (imported) {
				lib.macros[idx].trace();
				sch::Subckt gdsNet(true);
				extract(gdsNet, lib.macros[idx], true);
				gdsNet.cleanDangling(true);
				gdsNet.combineDevices();
				gdsNet.canonicalize();
				searchDelay = tmr.since();
				if (gdsNet.compare(spiNet) == 0) {
					printf("%sFOUND %d DBUNIT2 AREA%s]\t%gs\n", KGRN, lib.macros[idx].box.area(), KNRM, searchDelay);
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
			return true;
		} else {
			lib.macros[idx].clear();
		}
	}

	tmr.reset();

	int result = sch::buildCell(lib, lst, idx);
	if (progress) {
		if (result == 1) {
			genDelay = tmr.since();
			printf("%sFAILED PLACEMENT%s]\t(%gs %gs)\n", KRED, KNRM, searchDelay, genDelay);
		} else if (result == 2) {
			genDelay = tmr.since();
			printf("%sFAILED ROUTING%s]\t(%gs %gs)\n", KRED, KNRM, searchDelay, genDelay);
		} else {
			sch::Subckt gdsNet(true);
			extract(gdsNet, lib.macros[idx], true);
			gdsNet.cleanDangling(true);
			gdsNet.combineDevices();
			gdsNet.canonicalize();

			genDelay = tmr.since();
			if (gdsNet.compare(spiNet) == 0) {
				printf("%sGENERATED %d DBUNIT2 AREA%s]\t(%gs %gs)\n", KGRN, lib.macros[idx].box.area(), KNRM, searchDelay, genDelay);
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

void update_library(phy::Library &lib, sch::Netlist &lst, gdstk::GdsWriter *stream, map<int, gdstk::Cell*> *cells, bool progress, bool debug) {
	bool libFound = filesystem::exists(lib.tech->lib);
	if (progress) {
		printf("Load cell layouts:\n");
	}

	Timer tmr;
	lib.macros.reserve(lst.subckts.size()+lib.macros.size());
	for (int i = 0; i < (int)lst.subckts.size(); i++) {
		if (lst.subckts[i].isCell) {
			if (not import_cell(lib, lst, i, progress, debug)) {
				// We generated a new cell, save this to the cell library
				if (not libFound) {
					filesystem::create_directory(lib.tech->lib);
					libFound = true;
				}
				string cellPath = lib.tech->lib + "/" + lib.macros[i].name;
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
