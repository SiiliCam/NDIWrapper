#pragma once

#include <mutex>
#include <functional>
#include <Processing.NDI.Lib.h>
#include <vector>
#include <map>
#include <atomic>

#include "ThreadPool.hpp"
#include "MetaData.hpp"

/**
 * @brief base class for the ndi receiver and sender
 * @details keeps count of NDI objects for the purpose to 
 * * initialize the NDI LIB on first object
 * * destroy the NDI LIB on last object
 */

template<typename NDIInstanceType>
class NDIBase {
private:
    static int object_count;
    static std::mutex count_mutex;

public:

    using MetadataFunc = std::function<void(NDIlib_metadata_frame_t& metadata)>;
    using CaptureMetadataFunc = std::function<NDIlib_frame_type_e(NDIlib_metadata_frame_t& metadata)>;
    NDIBase(CaptureMetadataFunc capture, MetadataFunc send, MetadataFunc free);
    virtual ~NDIBase();

    template<typename... Args>
    void sendMetadata(const Args&... args);

    virtual void metadataThreadLoop();


    void stopMetadataListening();
    void startMetadataListening();
    void addMetadataCallback(MetaDataCallback callback);

protected:
    NDIInstanceType pNDIInstance_;
    std::mutex pndiMutex_;

    CaptureMetadataFunc captureMetadata_;
    MetadataFunc sendMetadata_;
    MetadataFunc freeMetadata_;
    ThreadPool threadPool_;

private:
    std::atomic<bool> metadatalistenerrunning_;
    std::thread metadataThread_;
    std::mutex metadataCallbackMutex_;
    std::vector<MetaDataCallback> _metadataCallbacks;
};


template<typename NDIInstanceType>
template<typename... Args>
void NDIBase<NDIInstanceType>::sendMetadata(const Args&... args)
{
    std::string encodedMetadata = Metadata::encode(args...);
    NDIlib_metadata_frame_t metadata;
    metadata.length = static_cast<int>(encodedMetadata.size()) + 1;  // +1 for null terminator
    metadata.p_data = new char[metadata.length];
    std::strcpy(metadata.p_data, encodedMetadata.c_str());

    {
        std::lock_guard<std::mutex> lock(pndiMutex_);
        sendMetadata_(metadata);
    }

    delete[] metadata.p_data;

}

// Explicit template instantiation for known types (if needed)
template class NDIBase<NDIlib_send_instance_t>;
template class NDIBase<NDIlib_recv_instance_t>;
