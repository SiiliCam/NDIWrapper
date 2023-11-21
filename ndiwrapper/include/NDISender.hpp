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

/**
 * @brief the sender implementation, sends iamge frames and also can receive and send metadataframes due to tha base
 * @details makes the sender seen with any receiver with the given name and group
 * ndi source is a ndi sender that is initalized as sender. When ndi sender is initialized it broadcasts the name
 * of the source (ndi sender) with mdns to the network and it can be seen by any ndi source listeners.
 * The source can be set to belong to a group, which can only be seen if you are listening to that specific group.
 */
class NDISender: public NDIBase<NDIlib_send_instance_t> {
public:
	/**
	 * @brief initializes the sender with the given name and group
	 * @param[in] name the mdns name of the sender
	 * @param[in] group the group of the sender
	 */
	NDISender(const std::string& name, const std::string& group = "", bool enableVideo = true, bool enableAudio = false);

	/**
	 * @brief stops the metadata listening and sending
	 */
	~NDISender();

	/**
	 * @brief feeds a Image to the ndi stream
	 * @param[in] image the image to send, this should include atleast the width,height,data and stride
	 * @param[in] videoType the frame type to add to the stream, by default it NV12 YUV (android camera is YUV by default)
	 */
	void feedFrame(Image& image, NDIlib_FourCC_video_type_e videoType = NDIlib_FourCC_video_type_NV12);
	void feedAudio(Audio& audioFrame);
	void asyncFeedFrame(const Image& image, NDIlib_FourCC_video_type_e videoType);

	void start();
	void stop();

private:
	std::vector<Image> m_frameBuffers;
	size_t m_currentBufferIndex = 0;
	std::mutex m_bufferMutex;
};