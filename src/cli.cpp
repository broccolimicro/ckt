#include "cli.h"

void set_stage(int &stage, int target) {
	stage = stage < target ? target : stage;
}
