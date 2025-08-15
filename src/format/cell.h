#pragma once

#include <sch/Netlist.h>
#include <phy/Library.h>

#include <interpret_sch/import.h>
#include <interpret_sch/export.h>
#include <interpret_phy/import.h>
#include <interpret_phy/export.h>

namespace cell {

void export_cell(int index, const phy::Library &lib, const sch::Netlist &net);
void export_cells(const phy::Library &lib, const sch::Netlist &net);
bool import_cell(phy::Library &lib, sch::Netlist &lst, int idx, bool progress=false, bool debug=false);
void update_library(phy::Library &lib, sch::Netlist &lst, gdstk::GdsWriter *stream=nullptr, map<int, gdstk::Cell*> *cells=nullptr, bool progress=false, bool debug=false);

}
