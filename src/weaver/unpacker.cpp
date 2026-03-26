#include "unpacker.h"

#include <common/standard.h>
#include <common/timer.h>
#include <common/text.h>

#include <filesystem>

Unpack::Unpack(weaver::Project &proj) : proj(proj) {
	stage = -1;
	progress = false;
	debug = false;

	targets.resize(SIZED+1, false);
}

Unpack::~Unpack() {
}

void Unpack::set(int target) {
	stage = stage < target ? target : stage;
	targets[target] = true;
}

bool Unpack::get(int target) const {
	return stage < 0 or stage >= target;
}

void Unpack::inclAll() {
	targets = vector<bool>(SIZED+1, true);
}

void Unpack::incl(int target) {
	targets[target] = true;
}

void Unpack::excl(int target) {
	targets[target] = false;
}

bool Unpack::has(int target) const {
	return targets[target];
}

