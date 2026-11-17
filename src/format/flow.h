#pragma once

#include <choy/project.h>
#include <flow/func.h>

choy::Prototype protoFromInstance(const choy::Program &prgm, const flow::Instance &inst, bool qualify=false);
