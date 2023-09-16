#include "NDIReceiver.hpp"
#include "Logger.hpp"

#include <iostream>
NDIReceiver::NDIReceiver(const std::string& groupToFind, bool findGroup) : threadPool_(4), isReceivingRunning_(false), isSourceFindingRunning_(false), metadatalistenerrunning_(false), _findGroup(findGroup), currentOutput_(), groupToFind_(groupToFind){
	// Initialization code, if any
	currentOutput_.p_ndi_name = "";
}

NDIReceiver::~NDIReceiver() {
	stop();
}

Image NDIReceiver::getFrame() {
	std::lock_guard<std::mutex> lock(frameMutex_);
	return currentFrame_;
}

void NDIReceiver::addFrameCallback(FrameCallback frameCallback) {
	std::lock_guard<std::mutex> lock(frameCallbackVecMutex_);
	_frameCallbacks.push_back(frameCallback);
}

void NDIReceiver::addNDISourceCallback(NDISourceCallback sourceCallback) {
	std::lock_guard<std::mutex> lock(ndiSourceCallbackMutex_);
	_ndiSourceCallbacks.push_back(sourceCallback);
}

void NDIReceiver::addMetadataCallback(MetaDataCallback metadataCallback) {
	std::lock_guard<std::mutex> lock(metadtaCallbackMutex_);
	_metadataCallbacks.push_back(metadataCallback);
}
void NDIReceiver::setFindOnlyGroupsState(bool state) {
	if (state != _findGroup.load()) {

		_findGroup = state;
		resetSources();
	}
}

void NDIReceiver::stopSourceFinding() {
	isSourceFindingRunning_.store(false);
	if (sourceThread_.joinable()) {
		sourceThread_.join();
	}
}

void NDIReceiver::startSourceFinding() {
	if (isSourceFindingRunning_.load() == false) {
		isSourceFindingRunning_ = true;
		sourceThread_ = std::thread(&NDIReceiver::updateSources, this);
	}
	else {
		Logger::log_warn("source finding is already running");
	}
}
void NDIReceiver::stopFrameGeneration() {
	isReceivingRunning_ = false;

	if (frameThread_.joinable()) {
		frameThread_.join();
	}
}
void NDIReceiver::startFrameGeneration() {
	isReceivingRunning_ = true;
	frameThread_ = std::thread(&NDIReceiver::generateFrames, this);
}

void NDIReceiver::stopMetadataListening() {
	metadatalistenerrunning_ = false;
	if (metadataThread_.joinable()) {
		metadataThread_.join();
	}
}
void NDIReceiver::startMetadataListening() {
	if (isSourceFindingRunning_.load() == false) {

		metadatalistenerrunning_ = true;
		metadataThread_ = std::thread(&NDIReceiver::updateSources, this);
	}
	else {
		Logger::log_warn("metadata listening already running");
	}

}

void NDIReceiver::start() {
	startSourceFinding();
	startFrameGeneration();
}

void NDIReceiver::stop() {
	Logger::log_info("stopping ndiwrapper");
	stopSourceFinding();
	stopFrameGeneration();

	std::lock_guard<std::mutex> lock(pndiMutex_);
	if (pNDI_recv_) {
		NDIlib_recv_destroy(pNDI_recv_);
		pNDI_recv_ = nullptr;
	}
}

void NDIReceiver::setOutput(const std::string& outputName) {
	Logger::log_info("setting output to", outputName);
	try {
		if (outputName == std::string(currentOutput_.p_ndi_name)) {
			Logger::log_info("output is the same as current, doing nothing");
			return;
		}
		std::unique_lock<std::mutex> sourceLock(sourceMutex_);
		decltype(ndiSources_.at(outputName)) ndisource = ndiSources_.at(outputName);
		currentOutput_ = ndisource;
		sourceLock.unlock();

		std::lock_guard<std::mutex> lock(pndiMutex_);
		if (pNDI_recv_) {
			NDIlib_recv_destroy(pNDI_recv_);
		}
		// Create a new receiver for the selected source
		Logger::log_info("connecting to output");
		NDIlib_recv_create_v3_t recv_desc;
		recv_desc.source_to_connect_to = currentOutput_; // Assuming selectedSource_ is of type NDIlib_source_t
		recv_desc.color_format = NDIlib_recv_color_format_e_RGBX_RGBA; // Example format
		pNDI_recv_ = NDIlib_recv_create_v3(&recv_desc);
		Logger::log_info("created connection");
	}
	catch (const std::exception& e) {
		Logger::log_error("could not set currentOutput_");
	}
	Logger::log_info("output set", outputName);
}

void NDIReceiver::resetSources() {
	stop();
	ndiSources_.clear();
	start();
}

void NDIReceiver::updateSources() {
	try {
		while (isSourceFindingRunning_.load()) {
			decltype(ndiSources_) tempSources;
			std::vector<std::string> newSources;
			NDIlib_find_create_t NDI_find_create_desc; /* Use defaults */
			if (_findGroup.load()) {
				NDI_find_create_desc.p_groups = groupToFind_.c_str();
			}
			NDIlib_find_instance_t pNDI_find = NDIlib_find_create_v2(&NDI_find_create_desc);
			if (!pNDI_find) {
				Logger::log_error("Failed to create NDI finder instance.");
				return;
			}
			for (int i = 0; i < 10 && isSourceFindingRunning_.load(); i++) {
				// Wait up till 2 seconds to check for new sources to be added or removed
				if (NDIlib_find_wait_for_sources(pNDI_find, 2000)) {
					// Get the updated list of sources
					uint32_t no_sources = 0;
					const NDIlib_source_t* p_sources = NDIlib_find_get_current_sources(pNDI_find, &no_sources);
					Logger::log_info("total sources found", no_sources);
					// Add the sources to our new list
					for (uint32_t i = 0; i < no_sources; i++) {
						Logger::log_info("found source", p_sources[i].p_ndi_name);
						tempSources[p_sources[i].p_ndi_name] = p_sources[i];
						{
							{
								std::lock_guard<std::mutex> lock(sourceMutex_);
								ndiSources_[p_sources[i].p_ndi_name] = p_sources[i];
							}
							std::lock_guard<std::mutex> lock(ndiSourceCallbackMutex_);
							for (const auto& callback : _ndiSourceCallbacks) {
								std::string sourceName = std::string(p_sources[i].p_ndi_name);
								threadPool_.enqueue([=]() { callback(sourceName); });
							}
						}
					}
				}
			}

			// Destroy the NDI finder instance
			NDIlib_find_destroy(pNDI_find);
			// Update the member variable with the new list

			{
				std::lock_guard<std::mutex> lock(sourceMutex_);
				ndiSources_ = tempSources;
			}
		}
	}
	catch (const std::exception& e) {
		Logger::log_error("failed to get sources", e.what());
	}

}

std::vector<std::string> NDIReceiver::getCurrentSources() {
	std::lock_guard<std::mutex> lock(sourceMutex_);
	std::vector<std::string> sources;
	for (const auto& pair : ndiSources_)
	{
		sources.push_back(pair.first);
	}
	return sources;
}

void NDIReceiver::generateFrames() {
	uint32_t sleeptimeMS = 16;

	try {
		while (isReceivingRunning_.load()) {

			// Check if the selected source has changed

			// Capture video frames from the receiver
			NDIlib_video_frame_v2_t video_frame;
			{
				std::lock_guard<std::mutex> lock(pndiMutex_);
				NDIlib_recv_capture_v2(pNDI_recv_, &video_frame, nullptr, nullptr, 1000); // 5-second timeout
			}
			if (video_frame.p_data) {
				// Convert the NDI frame to your desired format and store in currentFrame_
				Image frame;
				frame.width = video_frame.xres;
				frame.height = video_frame.yres;
				frame.channels = 4; // Assuming RGBA format
				frame.data.assign(video_frame.p_data, video_frame.p_data + video_frame.xres * video_frame.yres * frame.channels);

				{
					std::lock_guard<std::mutex> lock(frameMutex_);
					currentFrame_ = frame;
				}
				{
					std::lock_guard<std::mutex> lock(frameCallbackVecMutex_);
					for (const auto& callback : _frameCallbacks) {
						Image frameCopy = frame;  // Ensure you make a deep copy if needed
						threadPool_.enqueue([=]() { callback(frameCopy); });
					}
				}
				// Free the video frame after using it
				{
					std::lock_guard<std::mutex> lock(pndiMutex_);
					NDIlib_recv_free_video_v2(pNDI_recv_, &video_frame);
				}
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(sleeptimeMS)); // ~60fps
		}
	}
	catch (const std::exception& e) {
		Logger::log_error("Could not generate frames", e.what());
	}
}

void NDIReceiver::metadataThreadLoop() {
	try {
		while (metadatalistenerrunning_.load()) {
			bool isLegit = false;
			NDIlib_metadata_frame_t metaDataFrame;

			// Step 1: Lock and validate pNDI_recv_
			{
				std::lock_guard<std::mutex> lock(pndiMutex_);
				isLegit = !!pNDI_recv_;

				if (isLegit) {
					NDIlib_recv_capture_v2(pNDI_recv_, NULL, NULL, &metaDataFrame, 100);  // Assume metadataRecv populates frame
				}
			}

			// Step 2: Sleep if pNDI_recv_ is not legit
			if (!isLegit) {
				std::this_thread::sleep_for(std::chrono::seconds(1));
				continue;
			}

			// Step 3: Parse the metadata
			MetadataContainer container;
			try {

				container = Metadata::decode(metaDataFrame.p_data);  // Assuming frame.data is std::string
				// Step 4: Enqueue the parsed metadata for processing
				std::lock_guard<std::mutex> lock(metadtaCallbackMutex_);
				for (const auto& callback : _metadataCallbacks) {
					MetadataContainer containerCopy = container;  // Deep copy if needed
					threadPool_.enqueue([=]() { callback(containerCopy); });
				}
			}
			catch (const std::exception& e) {
				Logger::log_error("could not decode metadata", e.what());
			}
			{
				std::lock_guard<std::mutex> lock(pndiMutex_);
				NDIlib_recv_free_metadata(pNDI_recv_, &metaDataFrame);
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}
	catch (const std::exception& e) {
		Logger::log_error("Could not handle metadata", e.what());
	}
}

template<typename... Args>
inline void NDIReceiver::sendMetadata(const Args&... args) {
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
		throw std::runtime_error("Failed to send metadata");
	}
}