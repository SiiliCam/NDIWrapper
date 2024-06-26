#pragma once

#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <Processing.NDI.Lib.h>
#include <map>
#include <functional>
#include <optional>

#include "commontypes.hpp"
#include "ThreadPool.hpp"
#include "MetaData.hpp"
#include "AugmentedTypes.hpp"
#include "Logger.hpp"

#include "NDIBase.hpp"

using namespace common_types;

using FrameCallback = std::function<void(Image)>;
using NDISourceCallback = std::function<void(std::string)>;
using AudioCallback = std::function<void(Audio)>;
using FrameWithMetadataCallback = std::function<void(DataWithMetadata<Image>)>;

using ConnectionCallback = std::function<void()>;
using ConnectionCallbackAudio = std::function<void(Audio)>;
using ConnectionCallbackVideo = std::function<void(Image)>;

using NDIFrame = std::pair<std::optional<DataWithMetadata<Audio>>, std::optional<DataWithMetadata<Image>>>;
/**
 * @brief the receiver implementation, for sources and frames there is callbacks when new one comes
 * @details this listens to new ndi sources, metadata (because of base) and frames
 * ndi source is a ndi sender that is initalized as sender. When ndi sender is initialized it broadcasts the name
 * of the source (ndi sender) with mdns to the network and it can be seen by any ndi source listeners.
 * The source can be set to belong to a group, which can only be seen if you are listening to that specific group.
 *
 * In receiver we listen to all senders from a specific group which you can select any to connect to and start getting the metadata and image and audio frames
 * = you can listen only one group at a time with this implementation
 * 
 * Currently supports only image and metadata frames
 */
class NDIReceiver : public NDIBase<NDIlib_recv_instance_t> {
public:

	/**
	 * @brief initializes the ndi receiver and its members
	 * @param[in] groupToFind is the group that this receiver listens to
	 * @param[in] findGroup controls if we are actually finding the group. If this is set to false, then sources that have no group
	 * are added
	 */
	NDIReceiver(const std::string& groupToFind = "", bool findGroup = true, bool synced = false);

	/**
	 * @brief stops the source listening, and frame listening
	 */
	~NDIReceiver();

	/**
	 * @brief a method to get the current frame
	 * @returns the frame
	 */
	Image getFrame();

	/**
	 * @brief a method to get the audio
	 * @returns the audio frame
	 */
	Audio getAudio();

	/**
	 * @brief blocks until it manages to get a new audio
	 * @returns the audio frame
	 */
	Audio getAudioNew();

	/**
	 * @brief starts the source listening, frame listening and metadata listening
	 */
	void start();

	/**
	 * @brief stops the source listening, and frame listening
	 */
	void stop();

	/**
	 * @brief connects to the given output
	 * @param[in] outputName the mdns name of the source, this must be in the ndiSources_ map
	 * @returns true if it set new output and false if it did not do anything due to error or the source is already set
	 */
	bool setOutput(const std::string& outputName);

	/**
	 * @brief gets the current sources
	 * @returns the sources names gathered this far 
	 */
	std::vector<std::string> getCurrentSources();

	/**
	 * @brief stops the source listening thread
	 */
	void stopSourceFinding();

	/**
	 * @brief starts the source listening thread
	 */
	void startSourceFinding();

	/**
	 * @brief stops the frame listening thread
	 */
	void stopFrameGeneration();

	/**
	 * @brief starts the frame listening thread
	 */
	void startFrameGeneration();

	/**
	 * @brief adds a callback which gets called each time a new frame comes in
	 * @param[in] frameCallback the frame callback which gets called
	 * The frames are hard coded to be RGBA images
	 */
	void addFrameCallback(FrameCallback frameCallback);
	void addFrameWithMetadataCallback(FrameWithMetadataCallback frameCallback);
	/**
	 * @brief adds a callback which gets called each time a audio comes in
	 * @param[in] audioCallback the audio callback which gets called
	 */
	void addAudioCallback(AudioCallback audioCallback);

	/**
	 * @brief adds a frame callback
	 * @param[in] frameCallback the frame callback which gets called each time new frame comes in
	 */
	void addNDISourceCallback(NDISourceCallback sourceCallback);

	/**
	 * @brief switches between finding group defined in constructor
	 * @param[in] state true to get only the group defined sources false to get sources with no group
	 */
	void setFindOnlyGroupsState(bool state);

	/**
	 * @brief empties the ndiSources_ map 
	 */
	void resetSources();

	/**
	 * @brief gets called when we start receiving audio frames
	 * @note not synchronized, so set these before you start generating frames
	 */
	void setAudioConnectedCallback(ConnectionCallbackAudio callback);

	/**
	 * @brief gets called when we stop receiving audio frames
	 * @note not synchronized, so set these before you start generating frames
	 */
	void setAudioDisconnectedCallback(ConnectionCallback callback);

	/**
	 * @brief gets called when we start receiving video frames
	 * @note not synchronized, so set these before you start generating frames
	 */
	void setVideoConnectedCallback(ConnectionCallbackVideo callback);

	/**
	 * @brief gets called when we stop receiving video frames
	 * @note not synchronized, so set these before you start generating frames
	 */
	void setVideoDisconnectedCallback(ConnectionCallback callback);
private:
	/**
	 * @brief endless loop which updates the sources
	 * @details this calls every callback in \_ndiSourceCallbacks when a source is found and updates the ndiSources\_
	 */
	void updateSources();

	/**
	 * @brief endless loop waits for the video frames and audio to come from the selected source as they come
	 * @details calls every callback in \_frameCallbacks with the image and updates the currentFrame\_ with newest image
	 * The image is RGBA image and its currently hard coded
	 */
	void generateFrames();


	NDIFrame getFrameNDI();
	NDIFrame getFramesNDISynced();

	std::map<std::string, NDIlib_source_t> ndiSources_;

	std::mutex frameMutex_;
	std::mutex sourceMutex_;
	std::mutex audioMutex_;

	std::atomic<bool> isReceivingRunning_;
	std::atomic<bool> isSourceFindingRunning_;
	std::atomic<bool> isSourceSet_;

	std::condition_variable audioCondition_;
	bool audioAvailable_ = false;

	std::thread sourceThread_;
	std::thread frameThread_;

	Image currentFrame_;
	Audio currentAudio_;
	NDIlib_source_t currentOutput_;
	std::string currentOutputString_;

	std::vector<FrameCallback> _frameCallbacks;
	std::vector<NDISourceCallback> _ndiSourceCallbacks;
	std::vector<AudioCallback> _audioCallbacks;
	std::vector<FrameWithMetadataCallback> _frameWithMetadataCallbacks;

	ConnectionCallbackAudio _audioConnected;
	ConnectionCallback _audioDisconnected;

	ConnectionCallbackVideo _videoConnected;
	ConnectionCallback _videoDisconnected;

	std::mutex frameCallbackVecMutex_;
	std::mutex frameCallbackVecMutexMetadata_; // for sensor data currently
	std::mutex ndiSourceCallbackMutex_;
	std::mutex audioCallbackVecMutex_;

	std::atomic<bool> _findGroup;
	std::string groupToFind_;

	NDIlib_framesync_instance_t _pndiFrameSync;
	bool m_synced;

	std::atomic<bool> dontTryToSetSource_;
	std::mutex setOutputMutex_;
};

