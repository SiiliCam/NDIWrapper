#include "NDIReceiver.hpp"
#include "Logger.hpp"
#include <thread>
#include <chrono>
#include <algorithm>
#include <tuple>
#include "NDISender.hpp"
#include "MetaData.hpp"
#include <random>

int getRandomNumber(int length) {
    // Initialize a random number engine
    std::default_random_engine engine(static_cast<unsigned int>(std::chrono::steady_clock::now().time_since_epoch().count()));

    // Initialize a distribution from 0 to length
    std::uniform_int_distribution<int> distribution(0, length-1);

    // Generate and return a random number
    return distribution(engine);
}

std::tuple<double, double, double> calculateRGBMedians(const Image& image) {
    std::vector<uint8_t> redData, greenData, blueData;

    // Step 1: Separate channel data
    for (size_t i = 0; i < image.data.size(); i += 4) {
        redData.push_back(image.data[i]);
        greenData.push_back(image.data[i + 1]);
        blueData.push_back(image.data[i + 2]);
    }

    // Step 2: Sort channel data
    std::sort(redData.begin(), redData.end());
    std::sort(greenData.begin(), greenData.end());
    std::sort(blueData.begin(), blueData.end());

    // Step 3: Calculate median for each channel
    auto calculateMedian = [](const std::vector<uint8_t>& channelData) -> double {
        size_t dataSize = channelData.size();
        if (dataSize % 2 != 0) {
            return static_cast<double>(channelData[dataSize / 2]);
        }
        else {
            return (static_cast<double>(channelData[(dataSize / 2) - 1]) +
                static_cast<double>(channelData[dataSize / 2])) /
                2.0;
        }
        };

    double redMedian = calculateMedian(redData);
    double greenMedian = calculateMedian(greenData);
    double blueMedian = calculateMedian(blueData);

    return std::make_tuple(redMedian, greenMedian, blueMedian);
}
Image generateRGBAImage(int width, int height, uint8_t r, uint8_t g, uint8_t b) {
    Image img;
    img.width = width;
    img.height = height;
    img.channels = 4; // RGBA

    int totalSize = width * height * img.channels; // 4 channels: RGBA
    img.data.resize(totalSize);

    // Populate data
    for (int i = 0; i < totalSize; i += 4) {
        img.data[i] = r;
        img.data[i + 1] = g;
        img.data[i + 2] = b;
        img.data[i + 3] = 0; // alpha
    }

    return img;
}

int main() {
	NDIReceiver receiver("Testing", true);

	// sets the found source as current
	receiver.addNDISourceCallback([&receiver](std::string source) -> void {
		Logger::log_info("found source", source);
		});
	receiver.start();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    receiver.addMetadataCallback([](MetadataContainer container) -> void {
        if (container.zoom.has_value()) {
            Logger::log_info("RECEIVER got zoom : ", container.zoom.value());
        }
        if (container.aspectRatio.has_value()) {
            const auto& aspectRatio = container.aspectRatio.value();
            Logger::log_info("RECEIVER got aspect ratio height:", aspectRatio.height, "width", aspectRatio.width);
        }
        if (container.boundingBox.has_value()) {
            const auto& boundingBox = container.boundingBox.value();
            Logger::log_info("RECEIVER got bounding box, start x", boundingBox.start.x, "start y", boundingBox.start.y, "width", boundingBox.width, "height", boundingBox.width);
        }
        if (container.switchCamera.has_value()) {
            const auto& switchCamera = container.switchCamera.value();
            Logger::log_info("RECEIVER got switchCamera: ", container.switchCamera.value());
        }
        });
    std::thread outputSwitcher([&]() -> void {
        while (true) {

            auto outputs = receiver.getCurrentSources();
            if (outputs.size() < 2) {
                std::this_thread::sleep_for(std::chrono::milliseconds(400));
                continue;
            }
            auto output = outputs.at(getRandomNumber(outputs.size()));
            receiver.setOutput(output);
            std::this_thread::sleep_for(std::chrono::milliseconds(400));
        }
        });
    outputSwitcher.detach();
    std::vector<NDISender*> senders;

    for (int i = 0; i < 20; i++) {
        NDISender* sender = new NDISender{ "mymdnsname" + std::to_string(i), "Testing" };
        senders.push_back(sender);
    }
    NDISender sender("mymdnsname", "Testing");
    sender.addMetadataCallback([](MetadataContainer container) -> void {
        if (container.zoom.has_value()) {
            Logger::log_info("SENDER got zoom : ", container.zoom.value());
        }
        if (container.aspectRatio.has_value()) {
            const auto& aspectRatio = container.aspectRatio.value();
            Logger::log_info("SENDER got aspect ratio height:", aspectRatio.height, "width", aspectRatio.width);
        }
        if (container.boundingBox.has_value()) {
            const auto& boundingBox = container.boundingBox.value();
            Logger::log_info("SENDER got bounding box, start x", boundingBox.start.x, "start y", boundingBox.start.y, "width", boundingBox.width, "height", boundingBox.width);
        }
        if (container.switchCamera.has_value()) {
            const auto& switchCamera = container.switchCamera.value();
            Logger::log_info("SENDER got switchCamera: ", container.switchCamera.value());
        }
        });

    senders[0]->start();
    std::this_thread::sleep_for(std::chrono::seconds(5));
    BoundingBox box;
    box.start.y = 200;
    box.start.x = 250;
    box.width = 400;
    box.height = 200;
    senders[0]->sendMetadata(box);

    Zoom zoom{4.2f};
    receiver.sendMetadata(zoom);

    AspectRatio aspect{ 16,9 };
    receiver.sendMetadata(aspect);

    std::thread t([&]() -> void {

        int i = 0;
        while (true) {
            Zoom zoom{ (++i % 200) / 10.0 };
            receiver.sendMetadata(zoom);
            auto frame = receiver.getFrame();
            if (!frame.data.empty()) {
                auto [redMedian, greenMedian, blueMedian] = calculateRGBMedians(frame);
                Logger::log_info("red median:", redMedian, "green median:", greenMedian, "blue median:", blueMedian, "image width", frame.width, "image height", frame.height);

            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }});

    for (auto& sender : senders) {
        std::thread senderThread([&]() {

            // Generate a 200x200 red image with RGBA format
            Image img = generateRGBAImage(200, 200, 255, 0, 0);
            img.stride = img.width * img.channels;
            // stride = width * channels

            int i = 0;
            while (true) {
                BoundingBox box2;
                box2.start.y = i%200+200;
                box2.start.x = 250;
                box2.width = 400;
                box2.height = 200;
                sender->sendMetadata(box2);
                sender->feedFrame(img, NDIlib_FourCC_video_type_RGBA);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            });
        senderThread.detach();
    }
    
    t.join();
	return 0;
}