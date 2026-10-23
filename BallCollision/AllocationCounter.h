#pragma once
#include <cstdint>

// Number of global operator new calls since program start. The counting
// operator new lives in AllocationCounter.cpp and is linked only into the
// interactive application, where the frame benchmark reports it per frame.
std::uint64_t AllocationCount();
