#include "Logger.hpp"
#include "MetaData.hpp"
#include "NDIReceiver.hpp"
#include "NDISender.hpp"
#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <random>
#include <thread>
#include <tuple>

/**
 * @brief this file is a sample application where we stress test the library by
 * creating multiple sources and one listener for them.
 */
int getRandomNumber(int length) {
  // Initialize a random number engine
  std::default_random_engine engine(static_cast<unsigned int>(
      std::chrono::steady_clock::now().time_since_epoch().count()));

  // Initialize a distribution from 0 to length
  std::uniform_int_distribution<int> distribution(0, length - 1);

  // Generate and return a random number
  return distribution(engine);
}

std::tuple<double, double, double> calculateRGBMedians(const Image &image) {
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
  auto calculateMedian = [](const std::vector<uint8_t> &channelData) -> double {
    size_t dataSize = channelData.size();
    if (dataSize % 2 != 0) {
      return static_cast<double>(channelData[dataSize / 2]);
    } else {
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
Image generateRGBAImage(int width, int height, uint8_t r, uint8_t g,
                        uint8_t b) {
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
  Logger::Logger::getInstance().init_logging("log.txt");

  HMODULE hMod = GetModuleHandleA("Processing.NDI.Lib.x64.dll");
  if (hMod != NULL) {
    Logger::log_error("NDI DLL already loaded BEFORE env var set!");
  }

  _putenv_s("NDI_CONFIG_DIR", "C:\\Users\\Simo\\AppData\\Roaming\\siili3");
  NDIReceiver receiver("", false, true);
  // sets the found source as current
  receiver.addNDISourceCallback([&receiver](std::string source) -> void {
    Logger::log_info("found source", source);
  });
  receiver.start();
  std::this_thread::sleep_for(std::chrono::seconds(5));

  auto sender = std::make_shared<NDISender>("vitunnisti", "");
  std::thread senderThread([&]() {
    // Generate a 200x200 red image with RGBA format
    Image img = generateRGBAImage(200, 200, 255, 0, 0);
    img.stride = img.width * img.channels;
    // stride = width * channels

    int i = 0;
    while (true) {
      BoundingBox box2;
      box2.start.y = i % 200 + 200;
      box2.start.x = 250;
      box2.width = 400;
      box2.height = 200;
      sender->sendMetadata(box2);
      sender->asyncFeedFrame(img, NDIlib_FourCC_video_type_RGBA);
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  });
  senderThread.join();
  std::this_thread::sleep_for(std::chrono::seconds(5));

  return 0;
}