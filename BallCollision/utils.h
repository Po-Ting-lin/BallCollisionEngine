#pragma once
#include <chrono>
#include <iostream>

static inline std::chrono::system_clock::time_point getTime() {
    return std::chrono::system_clock::now();
}

static inline void printTime(std::chrono::system_clock::time_point t1, std::chrono::system_clock::time_point t2, std::string name) {
    std::chrono::duration<double> time_lapse = t2 - t1;
    std::cout << name << " time consume: " << time_lapse.count() << " s" << std::endl;
}