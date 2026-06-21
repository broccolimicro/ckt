#pragma once

#include <sch/Subckt.h>
#include <phy/Layout.h>
#include <weaver/program.h>

#include <interpret_sch/import.h>
#include <interpret_sch/export.h>
#include <interpret_phy/import.h>
#include <interpret_phy/export.h>

namespace cell {

void export_cell(std::string path, const phy::Tech &tech, const weaver::Term &term);
void export_cells(std::string path, const phy::Tech &tech, const weaver::Module &mod);
void export_cells(std::string path, const phy::Tech &tech, const weaver::Program &prgm);
bool import_cell(std::string path, const phy::Tech &tech, weaver::Term &term, array<int, 2> *idx=nullptr, bool progress=false, bool debug=false);
void update_library(std::string path, const phy::Tech &tech, weaver::Module &mod, bool progress=false, bool debug=false);

}
