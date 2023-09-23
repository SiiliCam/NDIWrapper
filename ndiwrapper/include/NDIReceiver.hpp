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

#include "NDIBase.hpp"

using namespace common_types;


using FrameCallback = std::function<void(Image)>;
using NDISourceCallback = std::function<void(std::string)>;

class NDIReceiver: public NDIBase<NDIlib_recv_instance_t> {
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


    void addFrameCallback(FrameCallback frameCallback);
    void addNDISourceCallback(NDISourceCallback sourceCallback);
    void setFindOnlyGroupsState(bool state);

    void resetSources();

private:
    void updateSources();
    void generateFrames();

    std::map<std::string, NDIlib_source_t> ndiSources_;

    std::mutex frameMutex_;

    std::mutex sourceMutex_;
    std::atomic<bool> isReceivingRunning_;
    std::atomic<bool> isSourceFindingRunning_;

    std::thread sourceThread_;
    std::thread frameThread_;

    Image currentFrame_;
    NDIlib_source_t currentOutput_;
    std::string currentSteamID_;

    std::vector<FrameCallback> _frameCallbacks;
    std::vector<NDISourceCallback> _ndiSourceCallbacks;

    std::mutex frameCallbackVecMutex_;
    std::mutex ndiSourceCallbackMutex_;

    std::atomic<bool> _findGroup;
    std::string groupToFind_;
};

