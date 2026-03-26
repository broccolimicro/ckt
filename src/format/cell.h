#pragma once

#include <sch/Netlist.h>
#include <phy/Library.h>

#include <interpret_sch/import.h>
#include <interpret_sch/export.h>
#include <interpret_phy/import.h>
#include <interpret_phy/export.h>

namespace cell {

void export_cell(std::string path, const phy::Library &lib, const sch::Netlist &net, int index);
void export_cells(std::string path, const phy::Library &lib, const sch::Netlist &net);
bool import_cell(std::string path, phy::Library &lib, sch::Netlist &lst, int idx, bool progress=false, bool debug=false);
void update_library(std::string path, phy::Library &lib, sch::Netlist &lst, gdstk::GdsWriter *stream=nullptr, map<int, gdstk::Cell*> *cells=nullptr, bool progress=false, bool debug=false);

}
