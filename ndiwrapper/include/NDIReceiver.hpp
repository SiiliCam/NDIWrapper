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

using namespace common_types;


using FrameCallback = std::function<void(Image)>;
using NDISourceCallback = std::function<void(std::string)>;
using MetaDataCallback = std::function<void(MetadataContainer)>;

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

    // Template function for sending metadata
    template<typename... Args>
    void sendMetadata(const Args&... args);

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

    std::atomic<bool> _findGroup;
    NDIlib_recv_instance_t pNDI_recv_ = nullptr;

    std::string groupToFind_;
};

