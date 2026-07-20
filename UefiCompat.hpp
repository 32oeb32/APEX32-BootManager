#pragma once

// EDK II headers expose C interfaces. C++ translation units must include them
// with C linkage so library functions and protocol GUIDs retain their ABI names.
extern "C" {
#include <Uefi.h>
}

