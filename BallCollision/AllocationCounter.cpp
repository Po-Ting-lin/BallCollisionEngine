#include "AllocationCounter.h"

#include <atomic>
#include <cstdlib>
#include <new>

namespace {
	std::atomic<std::uint64_t> allocationCount{ 0 };

	void* countedAllocate(std::size_t size) {
		allocationCount.fetch_add(1, std::memory_order_relaxed);
		if (void* memory = std::malloc(size > 0 ? size : 1))
			return memory;
		throw std::bad_alloc();
	}
}

std::uint64_t AllocationCount() {
	return allocationCount.load(std::memory_order_relaxed);
}

void* operator new(std::size_t size) {
	return countedAllocate(size);
}

void* operator new[](std::size_t size) {
	return countedAllocate(size);
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
	allocationCount.fetch_add(1, std::memory_order_relaxed);
	return std::malloc(size > 0 ? size : 1);
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
	allocationCount.fetch_add(1, std::memory_order_relaxed);
	return std::malloc(size > 0 ? size : 1);
}

void operator delete(void* memory) noexcept {
	std::free(memory);
}

void operator delete[](void* memory) noexcept {
	std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept {
	std::free(memory);
}

void operator delete[](void* memory, std::size_t) noexcept {
	std::free(memory);
}
