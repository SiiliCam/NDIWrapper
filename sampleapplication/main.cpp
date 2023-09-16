#include "NDIReceiver.hpp"
#include "Logger.hpp"
#include <thread>
#include <chrono>

int main() {
	NDIReceiver receiver("SiiliCam", true);
	receiver.addNDISourceCallback([](std::string source) -> void {
		Logger::log_info("found source", source);
		});
	receiver.start();

	std::this_thread::sleep_for(std::chrono::seconds(60));
	return 0;
}