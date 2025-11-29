#include "NDILibraryManager.hpp"

// In .cpp file - initialize statics
std::mutex NDILibraryManager::mutex_;
int NDILibraryManager::refCount_ = 0;
const NDIlib_v6 *NDILibraryManager::lib_ = nullptr;
#ifdef _WIN32
HMODULE NDILibraryManager::hNDI_ = nullptr;
#else
void *NDILibraryManager::hNDI_ = nullptr;
#endif