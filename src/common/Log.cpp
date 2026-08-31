#include "Log.h"

#include <cstdio>
#include <mutex>

namespace ext17::log {
namespace {

std::mutex g_mutex;
std::FILE* g_mirror = nullptr;

} // namespace

bool mirrorToFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_mirror) {
        std::fclose(g_mirror);
        g_mirror = nullptr;
    }
    g_mirror = std::fopen(path.c_str(), "wb");
    return g_mirror != nullptr;
}

void closeMirror() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_mirror) {
        std::fclose(g_mirror);
        g_mirror = nullptr;
    }
}

void line(const char* stage, const std::string& text) {
    std::lock_guard<std::mutex> lock(g_mutex);
    std::printf("[campaign] %-12s %s\n", stage, text.c_str());
    std::fflush(stdout);
    if (g_mirror) {
        std::fprintf(g_mirror, "[campaign] %-12s %s\n", stage, text.c_str());
        std::fflush(g_mirror);
    }
}

} // namespace ext17::log
