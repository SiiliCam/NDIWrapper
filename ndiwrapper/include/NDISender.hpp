#pragma once
#include "MetaData.hpp"
#include "ThreadPool.hpp"
#include "commontypes.hpp"
#include <Processing.NDI.Lib.h>
#include <atomic>
#include <mutex>
#include <thread>

#include "Logger.hpp"
#include "NDIBase.hpp"

using namespace common_types;

class NDISender: public NDIBase<NDIlib_send_instance_t> {
public:
	NDISender(const std::string& name, const std::string& group = "");
	~NDISender();
	void feedFrame(Image& image, NDIlib_FourCC_video_type_e videoType = NDIlib_FourCC_video_type_NV12);


	void start();
	void stop();
};