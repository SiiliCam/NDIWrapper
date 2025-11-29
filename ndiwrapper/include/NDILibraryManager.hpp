#pragma once

#include <Processing.NDI.Lib.h>

#include "Logger.hpp"
#include <Processing.NDI.DynamicLoad.h>
#include <mutex>
#ifdef _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

class NDILibraryManager {
public:
  static const NDIlib_v6 *Acquire() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (refCount_++ == 0) {
      // First acquisition - load the library
#ifdef _WIN32
      hNDI_ = LoadLibraryA("Processing.NDI.Lib.x64.dll");
      auto load_fn =
          (const NDIlib_v6 *(*)(void))GetProcAddress(hNDI_, "NDIlib_v6_load");
#else
      hNDI_ = dlopen("libndi.so", RTLD_NOW);
      auto load_fn = (const NDIlib_v6 *(*)(void))dlsym(hNDI_, "NDIlib_v6_load");
#endif
      if (!load_fn) {
        refCount_--;
        return nullptr;
      }
      lib_ = load_fn();
      if (lib_) {
        lib_->initialize();
      }
    }
    return lib_;
  }

  static void Release() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (--refCount_ == 0 && lib_) {
      // Last release - destroy and unload
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
    }
  }

private:
  static std::mutex mutex_;
  static int refCount_;
  static const NDIlib_v6 *lib_;
#ifdef _WIN32
  static HMODULE hNDI_;
#else
  static void *hNDI_;
#endif
};
