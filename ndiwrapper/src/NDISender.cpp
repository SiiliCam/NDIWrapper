#include "NDISender.hpp"
#include <exception>
#include <string>
#include <cstring>
#include <mutex>
#include <thread>

#include "Logger.hpp"

NDISender::NDISender(const std::string& name, const std::string& group) : threadPool_(2), metadatalistenerrunning_(false) {
	NDIlib_initialize();
	NDIlib_send_create_t NDI_send_create_desc;
	NDI_send_create_desc.p_ndi_name = name.c_str();
	if (group.length() > 0) {
		NDI_send_create_desc.p_groups = group.c_str(); // Use your actual group name here
	}
	NDI_send_create_desc.clock_video = true;
	NDI_send_create_desc.clock_audio = false;

	{
		std::lock_guard<std::mutex> lock(pndiMutex_);
		pNDI_send_ = NDIlib_send_create(&NDI_send_create_desc);
		if (!pNDI_send_) {
			throw std::runtime_error("Failed to create NDI send instance");
		}
	}
}

NDISender::~NDISender() {
	stop();
	std::lock_guard<std::mutex> lock(pndiMutex_);
	if (pNDI_send_) {
		NDIlib_send_destroy(pNDI_send_);
		pNDI_send_ = nullptr;
	}
	NDIlib_destroy();
}

void NDISender::start() {
	if (metadatalistenerrunning_.load() == false)
	{
		metadatalistenerrunning_ = true;
		metadataThread_ = std::thread(&NDISender::metadataThreadLoop, this);
	}
	else {
		Logger::log_error("metadatathread already running");
	}
}
void NDISender::stop() {
	metadatalistenerrunning_ = false;
	if (metadataThread_.joinable()) {
		metadataThread_.join();
	}
}

void NDISender::feedFrame(Image& image, int stride, NDIlib_FourCC_video_type_e videoType) {
	if (!pNDI_send_) {
		Logger::log_error("Pndi send not initialized");
	}
	NDIlib_video_frame_v2_t NDI_video_frame;
	NDI_video_frame.xres = image.width; // Assuming Image has a width member
	NDI_video_frame.yres = image.height; // Assuming Image has a height member
	NDI_video_frame.FourCC = videoType;
	NDI_video_frame.p_data = image.data.data(); // Assuming Image has a data member
	NDI_video_frame.line_stride_in_bytes = stride;

	{
		std::lock_guard<std::mutex> lock(pndiMutex_);
		NDIlib_send_send_video_v2(pNDI_send_, &NDI_video_frame);
	}
}

void NDISender::addMetadataCallback(MetaDataCallback callback) {
	std::lock_guard<std::mutex> lock(metadtaCallbackMutex_);
	_metadataCallbacks.push_back(callback);
}

void NDISender::metadataThreadLoop() {
	try {
		while (metadatalistenerrunning_.load()) {

			std::this_thread::sleep_for(std::chrono::milliseconds(10));

			// Step 1: Lock and validate pNDI_recv_
			{
				NDIlib_metadata_frame_t metaDataFrame;
				std::lock_guard<std::mutex> lock(pndiMutex_);
				bool isLegit = !!pNDI_send_;

				if (isLegit) {
					NDIlib_frame_type_e type;
					{
						type = NDIlib_send_capture(pNDI_send_, &metaDataFrame, 0);  // Assume metadataRecv populates frame
					}
					if (type != NDIlib_frame_type_e::NDIlib_frame_type_metadata) {
						continue;
					}
				}
				else {
					continue;
				}


				// Step 3: Parse the metadata
				MetadataContainer container;
				try {
					if (metaDataFrame.p_data) {
						container = Metadata::decode(metaDataFrame.p_data);  // Assuming frame.data is std::string
						// Step 4: Enqueue the parsed metadata for processing
						std::lock_guard<std::mutex> lock(metadtaCallbackMutex_);
						for (const auto& callback : _metadataCallbacks) {
							MetadataContainer containerCopy = container;  // Deep copy if needed
							threadPool_.enqueue([=]() { callback(containerCopy); });
						}
						NDIlib_send_free_metadata(pNDI_send_, &metaDataFrame);
					}
				}
				catch (const std::exception& e) {
					Logger::log_error("could not decode metadata", e.what());
				}
			}


		}
	}
	catch (const std::exception& e) {
		Logger::log_error("Could not handle metadata", e.what());
	}
}

