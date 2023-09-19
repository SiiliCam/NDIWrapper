#pragma once
#include "MetaData.hpp"
#include "ThreadPool.hpp"
#include "commontypes.hpp"
#include <Processing.NDI.Lib.h>
#include <atomic>
#include <mutex>
#include <thread>

#include "Logger.hpp"

using namespace common_types;

class NDISender {
public:
	NDISender(const std::string& name, const std::string& group = "");
	~NDISender();
	void feedFrame(Image& image, NDIlib_FourCC_video_type_e videoType = NDIlib_FourCC_video_type_NV12);
	void addMetadataCallback(MetaDataCallback callback);

	void metadataThreadLoop();

	template<typename... Args>
	void sendMetadata(const Args&... args) {
		std::string encodedMetadata = Metadata::encode(args...);
		NDIlib_metadata_frame_t metadata;
		metadata.length = static_cast<int>(encodedMetadata.size()) + 1;  // +1 for null terminator
		metadata.p_data = new char[metadata.length];
		std::strcpy(metadata.p_data, encodedMetadata.c_str());

		// Call your actual method to send metadata here
		{
			std::lock_guard<std::mutex> lock(pndiMutex_);

			NDIlib_send_send_metadata(pNDI_send_, &metadata);  // Assume metadataSend can accept std::string
			//Logger::log_info("sent data", metadata.p_data, "length", metadata.length);
		}
		delete[] metadata.p_data;

	}

	void start();
	void stop();
private:
	ThreadPool threadPool_;
	NDIlib_send_instance_t pNDI_send_ = nullptr;
	std::vector<MetaDataCallback> _metadataCallbacks;
	std::mutex metadtaCallbackMutex_;
	std::atomic<bool> metadatalistenerrunning_;
	std::mutex pndiMutex_;

	std::thread metadataThread_;

};