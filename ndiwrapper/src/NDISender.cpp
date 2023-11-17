#include "NDISender.hpp"
#include <exception>
#include <string>
#include <cstring>
#include <mutex>
#include <thread>

#include "Logger.hpp"

NDISender::NDISender(const std::string& name, const std::string& group, bool enableVideo, bool enableAudio) :
	NDIBase(
		[this](NDIlib_metadata_frame_t& metadataFrame) {
			return NDIlib_send_capture(pNDIInstance_, &metadataFrame, 0);
		},
		[this](NDIlib_metadata_frame_t& metadataFrame) {
			NDIlib_send_send_metadata(pNDIInstance_, &metadataFrame);
		},
		[this](NDIlib_metadata_frame_t& metadataFrame) {
			NDIlib_send_free_metadata(pNDIInstance_, &metadataFrame);
		}
	) {
	NDIlib_send_create_t NDI_send_create_desc;
	NDI_send_create_desc.p_ndi_name = name.c_str();
	if (group.length() > 0) {
		NDI_send_create_desc.p_groups = group.c_str(); // Use your actual group name here
	}
	NDI_send_create_desc.clock_video = enableVideo;
	NDI_send_create_desc.clock_audio = enableAudio;

	{
		std::lock_guard<std::mutex> lock(pndiMutex_);
		pNDIInstance_ = NDIlib_send_create(&NDI_send_create_desc);
		if (!pNDIInstance_) {
			throw std::runtime_error("Failed to create NDI send instance");
		}
	}
}

NDISender::~NDISender() {
	stop();
	std::lock_guard<std::mutex> lock(pndiMutex_);
	if (pNDIInstance_) {
		NDIlib_send_destroy(pNDIInstance_);
		pNDIInstance_ = nullptr;
	}
}

void NDISender::start() {
	startMetadataListening();

}
void NDISender::stop() {
	stopMetadataListening();

}

void NDISender::feedFrame(Image& image, NDIlib_FourCC_video_type_e videoType) {
	if (!pNDIInstance_) {
		Logger::log_error("Pndi send not initialized");
	}
	NDIlib_video_frame_v2_t NDI_video_frame;
	NDI_video_frame.xres = image.width; // Assuming Image has a width member
	NDI_video_frame.yres = image.height; // Assuming Image has a height member
	NDI_video_frame.FourCC = videoType;
	NDI_video_frame.p_data = image.data.data(); // Assuming Image has a data member
	NDI_video_frame.line_stride_in_bytes = image.stride;

	{
		std::lock_guard<std::mutex> lock(pndiMutex_);
		NDIlib_send_send_video_v2(pNDIInstance_, &NDI_video_frame);
	}
}
void NDISender::feedAudio(Audio& audio) {
	if (!pNDIInstance_) {
		Logger::log_error("Pndi send not initialized for audio");
		return;
	}

	NDIlib_audio_frame_v2_t NDI_audio_frame;
	NDI_audio_frame.sample_rate = audio.sampleRate;
	NDI_audio_frame.no_channels = audio.channels;
	NDI_audio_frame.no_samples = audio.noSamples;
	NDI_audio_frame.p_data = audio.data.data(); // Point to the data in the vector

	{
		std::lock_guard<std::mutex> lock(pndiMutex_);
		NDIlib_send_send_audio_v2(pNDIInstance_, &NDI_audio_frame);
	}
}