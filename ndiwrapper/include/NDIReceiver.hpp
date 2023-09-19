#pragma once

#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <Processing.NDI.Lib.h>
#include <map>
#include <functional>
#include "commontypes.hpp"
#include "ThreadPool.hpp"
#include "MetaData.hpp"
#include "Logger.hpp"

using namespace common_types;


using FrameCallback = std::function<void(Image)>;
using NDISourceCallback = std::function<void(std::string)>;

class NDIReceiver {
public:
    NDIReceiver(const std::string& groupToFind = "", bool findGroup = true);
    ~NDIReceiver();

    Image getFrame();
    void start();

    void stop();
    void setOutput(const std::string& outputName);
    std::vector<std::string> getCurrentSources();

    void stopSourceFinding();
    void startSourceFinding();

    void stopFrameGeneration();
    void startFrameGeneration();

    void stopMetadataListening();
    void startMetadataListening();

    void addFrameCallback(FrameCallback frameCallback);
    void addNDISourceCallback(NDISourceCallback sourceCallback);
    void addMetadataCallback(MetaDataCallback callback);

    void setFindOnlyGroupsState(bool state);

    void resetSources();
    void metadataThreadLoop();

    template<typename... Args>
    void sendMetadata(const Args&... args) {
        std::string encodedMetadata = Metadata::encode(args...);
        NDIlib_metadata_frame_t metadata;
        metadata.length = static_cast<int>(encodedMetadata.size()) + 1;  // +1 for null terminator
        metadata.p_data = new char[metadata.length];
        std::strcpy(metadata.p_data, encodedMetadata.c_str());

        bool success;

        // Call your actual method to send metadata here
        {
            std::lock_guard<std::mutex> lock(pndiMutex_);

            success = NDIlib_recv_send_metadata(pNDI_recv_, &metadata);  // Assume metadataSend can accept std::string
        }
        delete[] metadata.p_data;

        if (!success) {
            Logger::log_error("failed to send metadata");
        }
    }

private:
    void updateSources();
    void generateFrames();

    std::map<std::string, NDIlib_source_t> ndiSources_;
    ThreadPool threadPool_;

    std::mutex frameMutex_;

    std::mutex sourceMutex_;
    std::atomic<bool> isReceivingRunning_;
    std::atomic<bool> isSourceFindingRunning_;
    std::atomic<bool> metadatalistenerrunning_;

    std::mutex pndiMutex_;

    std::thread sourceThread_;
    std::thread frameThread_;
    std::thread metadataThread_;

    Image currentFrame_;
    NDIlib_source_t currentOutput_;
    std::string currentSteamID_;

    std::vector<FrameCallback> _frameCallbacks;
    std::vector<NDISourceCallback> _ndiSourceCallbacks;
    std::vector<MetaDataCallback> _metadataCallbacks;

    std::mutex frameCallbackVecMutex_;
    std::mutex ndiSourceCallbackMutex_;
    std::mutex metadtaCallbackMutex_;

    NDIlib_recv_instance_t pNDI_recv_ = nullptr;

    std::atomic<bool> _findGroup;
    std::string groupToFind_;
};

