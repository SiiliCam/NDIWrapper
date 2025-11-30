#pragma once

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <Processing.NDI.Lib.h>

#include "Logger.hpp"
#include "Processing.NDI.DynamicLoad.h"
#include <mutex>
#include <string>
#include <vector>

class NDILibraryManager {
public:
  static const NDIlib_v6 *Acquire() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (refCount_++ == 0) {
#ifdef _WIN32
      Logger::log_info("Loading NDI library (Windows)...");

      std::vector<std::string> searchPaths;

      // 1. Same folder as our plugin DLL
      std::string pluginDir = GetPluginDirectory();
      if (!pluginDir.empty()) {
        searchPaths.push_back(pluginDir + "\\Processing.NDI.Lib.x64.dll");
      }

      // 2. NDI Runtime environment variable (if user has NDI Tools installed)
      char ndiRuntimePath[MAX_PATH] = {0};
      size_t len = 0;
      if (getenv_s(&len, ndiRuntimePath, sizeof(ndiRuntimePath),
                   "NDI_RUNTIME_DIR_V6") == 0 &&
          len > 0) {
        searchPaths.push_back(std::string(ndiRuntimePath) +
                              "\\Processing.NDI.Lib.x64.dll");
      }

      // 3. Fallback to just the DLL name (relies on system PATH)
      searchPaths.push_back("Processing.NDI.Lib.x64.dll");

      // Try each path
      for (const auto &path : searchPaths) {
        Logger::log_info("Trying to load NDI from:", path);
        hNDI_ = LoadLibraryA(path.c_str());
        if (hNDI_ != NULL) {
          Logger::log_info("Successfully loaded NDI from:", path);
          break;
        } else {
          DWORD error = GetLastError();
          Logger::log_info("Failed to load from", path, "- error:", error);
        }
      }

      if (hNDI_ == NULL) {
        DWORD error = GetLastError();
        Logger::log_error("Could not load Processing.NDI.Lib.x64.dll from any "
                          "location, last error:",
                          error);
        refCount_--;
        return nullptr;
      }

      auto load_fn =
          (const NDIlib_v6 *(*)(void))GetProcAddress(hNDI_, "NDIlib_v6_load");
#else
      Logger::log_info("Loading NDI library (Linux)...");
      hNDI_ = dlopen("libndi.so", RTLD_NOW);
      if (hNDI_ == nullptr) {
        Logger::log_error("Could not load libndi.so:", dlerror());
        refCount_--;
        return nullptr;
      }
      auto load_fn = (const NDIlib_v6 *(*)(void))dlsym(hNDI_, "NDIlib_v6_load");
#endif

      if (!load_fn) {
        Logger::log_error("NDIlib_v6_load function not found");
#ifdef _WIN32
        FreeLibrary(hNDI_);
#else
        dlclose(hNDI_);
#endif
        hNDI_ = nullptr;
        refCount_--;
        return nullptr;
      }

      lib_ = load_fn();
      if (lib_) {
        lib_->initialize();
        Logger::log_info("NDI initialized successfully");
      } else {
        Logger::log_error("NDIlib_v6_load returned nullptr");
        refCount_--;
        return nullptr;
      }
    }
    return lib_;
  }

  static void Release() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (--refCount_ == 0 && lib_) {
      Logger::log_info("Releasing NDI library...");
      lib_->destroy();
      lib_ = nullptr;
#ifdef _WIN32
      if (hNDI_) {
        FreeLibrary(hNDI_);
        hNDI_ = nullptr;
      }
#else
      if (hNDI_) {
        dlclose(hNDI_);
        hNDI_ = nullptr;
      }
#endif
      Logger::log_info("NDI library released");
    }
  }

private:
#ifdef _WIN32
  static std::string GetPluginDirectory() {
    char path[MAX_PATH] = {0};
    HMODULE hModule = NULL;

    // Get handle to THIS DLL using address of a static function in it
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)&GetPluginDirectory, &hModule)) {
      GetModuleFileNameA(hModule, path, MAX_PATH);
      std::string dir(path);
      size_t lastSlash = dir.find_last_of("\\/");
      if (lastSlash != std::string::npos) {
        return dir.substr(0, lastSlash);
      }
    }
    return "";
  }
#endif

  static std::mutex mutex_;
  static int refCount_;
  static const NDIlib_v6 *lib_;
#ifdef _WIN32
  static HMODULE hNDI_;
#else
  static void *hNDI_;
#endif
};
