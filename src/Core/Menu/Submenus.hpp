#pragma once

#include <vsh/vshcommon.hpp>
#include "Base.hpp"

void MainSubmenu();
void OptionsSubmenu();
void UpdateDnsFlow();
void ApplyDnsServer(size_t index);

extern "C" {
	#include "../../Utils/xregistry.h"
}
