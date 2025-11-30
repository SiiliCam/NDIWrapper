#pragma once

#include "Logger.hpp"
#include <NDILibraryManager.hpp>
#include <Processing.NDI.Lib.h>
#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

#include "MetaData.hpp"
#include "ThreadPool.hpp"

/**
 * @brief base class for the ndi receiver and sender, includes the metadata
 * handling methods for listening in thread and callbacks
 * @details keeps count of NDI objects for the purpose to initialize the NDI LIB
 * on first object creating and destroys the NDI LIB on last object. Metadata
 * handling is made with callbacks and the point is that when metadata comes it
 * calls callback containing the metadata info with which the caller can do what
 * ever the caller wishes Everything is wrapped in mutexes which makes this
 * thread safe
 *
 * Currently you cannot remove the metadatacallbacks which is probably fine
 * because user literally can just delete the object and create new if the
 * objects in callback is affecting get out of scope / get destroyed
 */

template <typename NDIInstanceType> class NDIBase {
public:
  using MetadataFunc = std::function<void(NDIlib_metadata_frame_t &metadata)>;
  using CaptureMetadataFunc =
      std::function<NDIlib_frame_type_e(NDIlib_metadata_frame_t &metadata)>;

  /**
   * @brief creates the NDI Lib if its the first object and initializes the
   * metadata functions
   * @details all of the parameters are receiver sender specific, thats why they
   * must be given
   * @param[in] capture the capture method this takes the metadata frame out of
   * the ndi stream
   * @param[in] send this send the given metadata frame through the ndi stream
   * @param[in] free this frees the given metadataframe from the ndi stream eg.
   * removes it
   */
  NDIBase(CaptureMetadataFunc capture, MetadataFunc send, MetadataFunc free);

  /**
   * @brief destroys the NDI lib if its the last object
   */
  virtual ~NDIBase();

  /**
   * @brief sends metadata through the ndi stream
   * @param[in] args contains types defined in MetaData.hpp, and then they are
   * serialized into xml format
   * @details serialized xml strings are sent through the ndi stream
   */
  template <typename... Args> void sendMetadata(const Args &...args);

  /**
   * @brief this captures the metadata from ndi stream, checks for errors and
   * calls the callbacks added to this object
   * @details is a loop that tries to read metadata from the ndi stream and then
   * sleeps for a time before trying to read again this uses captureMetadata_
   * and freeMetadata_ functions given in the constructor since they are
   * receiver/sender specific Also this way one can implement their own timeout
   * in the captureMetadata_ callback
   */
  void metadataThreadLoop();

  /**
   * @brief stops the metadatalistening thread gracefully
   */
  void stopMetadataListening();

  /**
   * @brief starts the metadata listening thread
   * @details starts the metadataThreadLoop in a thread and keeps the thread
   * handle in metadataThread_
   */
  void startMetadataListening();

  /**
   * @brief adds metadata callbacks
   * @details no remove function for them
   */
  void addMetadataCallback(MetaDataCallback callback);

protected:
  NDIInstanceType pNDIInstance_;
  std::mutex pndiMutex_;

  CaptureMetadataFunc captureMetadata_;
  MetadataFunc sendMetadata_;
  MetadataFunc freeMetadata_;
  ThreadPool threadPool_;
  const NDIlib_v6 *lib;

private:
  std::atomic<bool> metadatalistenerrunning_;
  std::thread metadataThread_;
  std::mutex metadataCallbackMutex_;
  std::vector<MetaDataCallback> _metadataCallbacks;
};

template <typename NDIInstanceType>
template <typename... Args>
void NDIBase<NDIInstanceType>::sendMetadata(const Args &...args) {
  std::string encodedMetadata = Metadata::encode(args...);
  NDIlib_metadata_frame_t metadata;
  metadata.length =
      static_cast<int>(encodedMetadata.size()) + 1; // +1 for null terminator
  metadata.p_data = new char[metadata.length];
  std::strcpy(metadata.p_data, encodedMetadata.c_str());

  {
    std::lock_guard<std::mutex> lock(pndiMutex_);
    sendMetadata_(metadata);
  }

  delete[] metadata.p_data;
}

template <typename NDIInstanceType>
NDIBase<NDIInstanceType>::NDIBase(CaptureMetadataFunc capture,
                                  MetadataFunc send, MetadataFunc free)
    : metadatalistenerrunning_(false), pNDIInstance_(nullptr),
      captureMetadata_(capture), sendMetadata_(send), freeMetadata_(free),
      threadPool_(4) {
  auto configDir = getenv("NDI_CONFIG_DIR");
  if (configDir != NULL) {
    Logger::log_info("NDI_CONFIG_DIR at NDIBASE: ", configDir);
  } else {
    Logger::log_info("NDI_CONFIG_DIR IS NOT SET IN NDIBASE");
  }

  lib = NDILibraryManager::Acquire(); // Loads + initializes if first
}
template <typename NDIInstanceType> NDIBase<NDIInstanceType>::~NDIBase() {
  NDILibraryManager::Release();
}

template <typename NDIInstanceType>
void NDIBase<NDIInstanceType>::metadataThreadLoop() {
  try {
    while (metadatalistenerrunning_.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));

      NDIlib_metadata_frame_t metaDataFrame;
      {
        std::lock_guard<std::mutex> lock(pndiMutex_);
        if (!pNDIInstance_)
          continue;

        NDIlib_frame_type_e type = captureMetadata_(metaDataFrame);

        if (type != NDIlib_frame_type_e::NDIlib_frame_type_metadata)
          continue;
      }

      try {
        if (metaDataFrame.p_data) {
          MetadataContainer container = Metadata::decode(metaDataFrame.p_data);

          std::lock_guard<std::mutex> lock(metadataCallbackMutex_);
          for (const auto &callback : _metadataCallbacks) {
            MetadataContainer containerCopy = container;
            threadPool_.enqueue([=]() { callback(containerCopy); });
          }

          freeMetadata_(metaDataFrame);
        }
      } catch (const std::exception &e) {
        Logger::log_error("could not decode metadata", e.what());
      }
    }
  } catch (const std::exception &e) {
    Logger::log_error("Could not handle metadata", e.what());
  }
}

template <typename NDIInstanceType>
void NDIBase<NDIInstanceType>::stopMetadataListening() {
  metadatalistenerrunning_ = false;
  if (metadataThread_.joinable()) {
    metadataThread_.join();
  }
  Logger::log_info("Metadata stopped");
}

template <typename NDIInstanceType>
void NDIBase<NDIInstanceType>::startMetadataListening() {
  if (metadatalistenerrunning_.load() == false) {

    metadatalistenerrunning_.store(true);
    metadataThread_ = std::thread(&NDIBase::metadataThreadLoop, this);
    Logger::log_info("Metadata started");
  } else {
    Logger::log_warn("metadata listening already running");
  }
}

template <typename NDIInstanceType>
void NDIBase<NDIInstanceType>::addMetadataCallback(
    MetaDataCallback metadataCallback) {
  std::lock_guard<std::mutex> lock(metadataCallbackMutex_);
  _metadataCallbacks.push_back(metadataCallback);
}

// Explicit template instantiation for known types (if needed)
template class NDIBase<NDIlib_send_instance_t>;
template class NDIBase<NDIlib_recv_instance_t>;
