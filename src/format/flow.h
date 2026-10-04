#pragma once

#include <weaver/project.h>
#include <flow/func.h>

weaver::Prototype protoFromInstance(const weaver::Program &prgm, const flow::Instance &inst, bool qualify=false);
