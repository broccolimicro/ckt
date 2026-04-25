#include "asic.h"

phy::Tech *loadASIC(weaver::Project &proj) {
	if (not proj.tech.def.has_value()) {
		proj.tech.def = make_any<phy::Tech>();
	}
	phy::Tech *tech = proj.tech.as<phy::Tech>();
	if (not tech->isLoaded() and not phy::loadTech(tech, proj.tech.path, proj.tech.args)) {
		cout << "Unable to load techfile \'" + proj.tech.path + "\'." << endl;
		return nullptr;
	}
	return tech;
}
