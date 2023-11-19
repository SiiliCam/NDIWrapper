#include "NDIReceiver.hpp"
#include "Logger.hpp"

#include <iostream>
NDIReceiver::NDIReceiver(const std::string& groupToFind, bool findGroup) :
	NDIBase(
		[this](NDIlib_metadata_frame_t& metadataFrame) {
			return NDIlib_recv_capture_v2(pNDIInstance_, nullptr, nullptr, &metadataFrame, 0);
		},
		[this](NDIlib_metadata_frame_t& metadataFrame) {
			NDIlib_recv_send_metadata(pNDIInstance_, &metadataFrame);
		},
		[this](NDIlib_metadata_frame_t& metadataFrame) {
			NDIlib_recv_free_metadata(pNDIInstance_, &metadataFrame);
		}
	),
	isReceivingRunning_(false),
	isSourceFindingRunning_(false),
	isSourceSet_(false),
	_findGroup(findGroup),
	currentOutput_(),
	groupToFind_(groupToFind) {
	// Initialization code, if any
	Logger::init_logging("C:/Users/Simo/AppData/Roaming/log2.log");
	currentOutput_.p_ndi_name = "";

}

NDIReceiver::~NDIReceiver() {
	stop();
}

Image NDIReceiver::getFrame() {
	std::lock_guard<std::mutex> lock(frameMutex_);
	return currentFrame_;
}

Audio NDIReceiver::getAudio() {
	std::lock_guard<std::mutex> lock(audioMutex_);
	auto audio = currentAudio_;
	currentAudio_.isNew = false;
	return audio;
}

void NDIReceiver::addFrameCallback(FrameCallback frameCallback) {
	std::lock_guard<std::mutex> lock(frameCallbackVecMutex_);
	_frameCallbacks.push_back(frameCallback);
}

void NDIReceiver::addNDISourceCallback(NDISourceCallback sourceCallback) {
	std::lock_guard<std::mutex> lock(ndiSourceCallbackMutex_);
	_ndiSourceCallbacks.push_back(sourceCallback);
}

void NDIReceiver::addAudioCallback(AudioCallback audioCallback) {
	std::lock_guard<std::mutex> lock(audioCallbackVecMutex_);
	_audioCallbacks.push_back(audioCallback);
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
	if (isReceivingRunning_.load()) {
		Logger::log_warn("frame generation already running");
		return;
	}
	isReceivingRunning_ = true;
	frameThread_ = std::thread(&NDIReceiver::generateFrames, this);
}

void NDIReceiver::start() {

	startSourceFinding();
	startFrameGeneration();
	startMetadataListening();
}

void NDIReceiver::stop() {
	Logger::log_info("stopping receiver");
	stopMetadataListening();
	stopSourceFinding();
	stopFrameGeneration();

	std::lock_guard<std::mutex> lock(pndiMutex_);
	if (pNDIInstance_) {
		NDIlib_recv_destroy(pNDIInstance_);
		pNDIInstance_ = nullptr;
	}
}


void NDIReceiver::setAudioConnectedCallback(ConnectionCallback callback) {
	_audioConnected = callback;
}

void NDIReceiver::setAudioDisconnectedCallback(ConnectionCallback callback) {
	_audioDisconnected = callback;
}

void NDIReceiver::setVideoConnectedCallback(ConnectionCallback callback) {
	_videoConnected = callback;
}

void NDIReceiver::setVideoDisconnectedCallback(ConnectionCallback callback) {
	_videoDisconnected = callback;
}

bool NDIReceiver::setOutput(const std::string& outputName) {
	Logger::log_info("setting output to", outputName);
	try {
		if (outputName == std::string(currentOutput_.p_ndi_name)) {
			Logger::log_info("output is the same as current, doing nothing");
			return false;
		}
		std::unique_lock<std::mutex> sourceLock(sourceMutex_);
		currentOutputString_ = outputName;
		decltype(ndiSources_.at(outputName)) ndisource = ndiSources_.at(outputName);
		currentOutput_ = ndisource;

		sourceLock.unlock();

		std::lock_guard<std::mutex> lock(pndiMutex_);
		if (pNDIInstance_) {
			NDIlib_recv_destroy(pNDIInstance_);
		}
		// Create a new receiver for the selected source
		Logger::log_info("connecting to output");
		NDIlib_recv_create_v3_t recv_desc;
		recv_desc.source_to_connect_to = currentOutput_; // Assuming selectedSource_ is of type NDIlib_source_t
		recv_desc.color_format = NDIlib_recv_color_format_e_RGBX_RGBA; // Example format
		pNDIInstance_ = NDIlib_recv_create_v3(&recv_desc);
		Logger::log_info("created connection");
		isSourceSet_ = true;
	}
	catch (const std::exception& e) {
		Logger::log_error("could not set currentOutput_: {}", e.what());
		isSourceSet_ = false;
		return false;
	}
	Logger::log_info("output set", outputName);
	return true;
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
	auto lastVideoFrameTime = std::chrono::steady_clock::now();
	auto lastAudioFrameTime = std::chrono::steady_clock::now();
	const auto disconnectionThreshold = std::chrono::seconds(3);

	bool videoConnected = false;
	bool audioConnected = false;
	try {
		while (isReceivingRunning_.load()) {

			// Check if the selected source has changed
			if (!isSourceSet_.load()) {
				if (!setOutput(currentOutputString_)) {
					std::this_thread::sleep_for(std::chrono::milliseconds(sleeptimeMS * 10));
					continue;
				}
			}
			// Capture video frames from the receiver
			{

				NDIlib_video_frame_v2_t video_frame;
				NDIlib_audio_frame_v2_t audio_frame;

				std::lock_guard<std::mutex> lock(pndiMutex_);
				auto type = NDIlib_recv_capture_v2(pNDIInstance_, &video_frame, &audio_frame, nullptr, 1000); // 5-second timeout

				if (type == NDIlib_frame_type_e::NDIlib_frame_type_metadata) {
					Logger::log_error("received metadata in generateframes ");
				}
				if (type == NDIlib_frame_type_e::NDIlib_frame_type_audio) {
					if (!audioConnected) {
						audioConnected = true;
						if (_audioConnected) {
							_audioConnected();
						}
					}

					lastAudioFrameTime = std::chrono::steady_clock::now();
					// Process and convert the NDI audio frame to your Audio struct
					Audio audio;
					audio.sampleRate = audio_frame.sample_rate;
					audio.channels = audio_frame.no_channels;
					audio.noSamples = audio_frame.no_samples;
					audio.data.assign(audio_frame.p_data, audio_frame.p_data + audio_frame.no_samples * audio_frame.no_channels);
					audio.isNew = true;
					NDIlib_recv_free_audio_v2(pNDIInstance_, &audio_frame);

					{
						std::lock_guard<std::mutex> lock(audioMutex_);
						currentAudio_ = audio;
					}
					// Call audio frame callbacks
					{
						std::lock_guard<std::mutex> lock(audioCallbackVecMutex_);
						for (const auto& callback : _audioCallbacks) {
							threadPool_.enqueue([=]() {
								callback(audio);
								});
						}
					}
				}
				else if (audioConnected && (std::chrono::steady_clock::now() - lastAudioFrameTime > disconnectionThreshold)) {
					audioConnected = false;
					if (_audioDisconnected) {
						_audioDisconnected();
					}
				}

				if (type == NDIlib_frame_type_e::NDIlib_frame_type_video) {
					if (!videoConnected) {
						videoConnected = true;
						if (_videoConnected) {
							_videoConnected();
						}
					}
					lastVideoFrameTime = std::chrono::steady_clock::now();
					// Convert the NDI frame to your desired format and store in currentFrame_
					Image frame;
					frame.width = video_frame.xres;
					frame.height = video_frame.yres;
					frame.channels = 4; // Assuming RGBA format

					frame.data.assign(video_frame.p_data, video_frame.p_data + video_frame.xres * video_frame.yres * frame.channels);

					NDIlib_recv_free_video_v2(pNDIInstance_, &video_frame);

					{
						std::lock_guard<std::mutex> lock(frameMutex_);
						currentFrame_ = frame;
					}
					{
						std::lock_guard<std::mutex> lock(frameCallbackVecMutex_);
						for (const auto& callback : _frameCallbacks) {
							threadPool_.enqueue([=]() {
								callback(frame);
								});
						}
					}
				}
				else if (videoConnected && (std::chrono::steady_clock::now() - lastVideoFrameTime > disconnectionThreshold)) {
					videoConnected = false;
					if (_videoDisconnected) {
						_videoDisconnected();
					}
				}
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
	catch (const std::exception& e) {
		Logger::log_error("Could not generate frames", e.what());
	}
}
