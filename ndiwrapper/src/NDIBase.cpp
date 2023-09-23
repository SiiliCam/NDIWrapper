#include "NDIBase.hpp"
#include "Logger.hpp"


template<typename NDIInstanceType>
NDIBase<NDIInstanceType>::NDIBase(CaptureMetadataFunc capture, MetadataFunc send, MetadataFunc free)
    : pNDIInstance_(nullptr), captureMetadata_(capture), sendMetadata_(send), freeMetadata_(free), threadPool_(4)
{
    std::lock_guard<std::mutex> lock(count_mutex);
    if (object_count == 0) {
        NDIlib_initialize();
    }
    ++object_count;
}

template<typename NDIInstanceType>
NDIBase<NDIInstanceType>::~NDIBase()
{
    std::lock_guard<std::mutex> lock(count_mutex);
    --object_count;
    if (object_count == 0) {
        NDIlib_destroy();
    }
}

template<typename NDIInstanceType>
void NDIBase<NDIInstanceType>::metadataThreadLoop() {
    try {
        while (metadatalistenerrunning_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            NDIlib_metadata_frame_t metaDataFrame;
            {
                std::lock_guard<std::mutex> lock(pndiMutex_);
                if (!pNDIInstance_) continue;

                NDIlib_frame_type_e type = captureMetadata_(metaDataFrame);

                if (type != NDIlib_frame_type_e::NDIlib_frame_type_metadata) continue;
            }

            try {
                if (metaDataFrame.p_data) {
                    MetadataContainer container = Metadata::decode(metaDataFrame.p_data);

                    std::lock_guard<std::mutex> lock(metadataCallbackMutex_);
                    for (const auto& callback : _metadataCallbacks) {
                        MetadataContainer containerCopy = container;
                        threadPool_.enqueue([=]() { callback(containerCopy); });
                    }

                    freeMetadata_(metaDataFrame);
                }
            }
            catch (const std::exception& e) {
                Logger::log_error("could not decode metadata", e.what());
            }
        }
    }
    catch (const std::exception& e) {
        Logger::log_error("Could not handle metadata", e.what());
    }
}


template<typename NDIInstanceType>
void NDIBase<NDIInstanceType>::stopMetadataListening() {
    metadatalistenerrunning_ = false;
    if (metadataThread_.joinable()) {
        metadataThread_.join();
    }
}

template<typename NDIInstanceType>
void NDIBase<NDIInstanceType>::startMetadataListening() {
    if (metadatalistenerrunning_.load() == false) {

        metadatalistenerrunning_.store(true);
        metadataThread_ = std::thread(&NDIBase::metadataThreadLoop, this);
    }
    else {
        Logger::log_warn("metadata listening already running");
    }

}

template<typename NDIInstanceType>
void NDIBase<NDIInstanceType>::addMetadataCallback(MetaDataCallback metadataCallback) {
    std::lock_guard<std::mutex> lock(metadataCallbackMutex_);
    _metadataCallbacks.push_back(metadataCallback);
}

// Initialize static members
template<typename NDIInstanceType>
int NDIBase<NDIInstanceType>::object_count = 0;

template<typename NDIInstanceType>
std::mutex NDIBase<NDIInstanceType>::count_mutex;
