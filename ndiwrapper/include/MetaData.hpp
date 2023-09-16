#pragma once
#include <boost/json.hpp>
#include <string>
#include <optional>

struct BoundingBox {
	struct {
		int64_t x;
		int64_t y;
	} start;
	int64_t width;
	int64_t height;
};

using Zoom = double;
using SwitchCamera = bool;

struct AspectRatio {
	int64_t width;
	int64_t height;
};

struct MetadataContainer {
	std::optional<BoundingBox> boundingBox;
	std::optional<Zoom> zoom;
	std::optional<SwitchCamera> switchCamera;
	std::optional<AspectRatio> aspectRatio;
};

namespace Metadata {
	template<typename T>
	inline void addToJson(boost::json::object& jsonObj, const T& data);

	template<>
	inline void addToJson<BoundingBox>(boost::json::object& jsonObj, const BoundingBox& box) {
		jsonObj["BoundingBox"] = {
				{"start", {{"x", box.start.x}, {"y", box.start.y}}},
				{"width", box.width},
				{"height", box.height} };
	}

	template<>
	inline void addToJson<Zoom>(boost::json::object& jsonObj, const Zoom& zoom) {
		jsonObj["Zoom"] = zoom;
	}

	template<>
	inline void addToJson<SwitchCamera>(boost::json::object& jsonObj, const SwitchCamera& switchCamera) {
		jsonObj["SwitchCamera"] = switchCamera;
	}

	template<>
	inline void addToJson<AspectRatio>(boost::json::object& jsonObj, const AspectRatio& aspectRatio) {
		jsonObj["AspectRatio"] = { {"width", aspectRatio.width}, {"height", aspectRatio.height} };
	}

	template<typename... Args>
	inline std::string encode(const Args&... args) {
		boost::json::object jsonObj;
		(addToJson(jsonObj, args), ...);
		return boost::json::serialize(jsonObj);
	}

	inline MetadataContainer decode(const std::string& jsonStr) {
		MetadataContainer container;
		boost::json::object jsonObj = boost::json::parse(jsonStr).as_object();

		// Note: Explicitly cast to boost::json::object
		if (jsonObj.contains("BoundingBox")) {
			boost::json::object boxObj = jsonObj["BoundingBox"].as_object();
			BoundingBox box;
			box.start.x = boxObj["start"].as_object()["x"].as_int64();
			box.start.y = boxObj["start"].as_object()["y"].as_int64();
			box.width = boxObj["width"].as_int64();
			box.height = boxObj["height"].as_int64();
			container.boundingBox = box;
		}

		if (jsonObj.contains("Zoom")) {
			container.zoom = jsonObj["Zoom"].as_double();
		}

		if (jsonObj.contains("SwitchCamera")) {
			container.switchCamera = jsonObj["SwitchCamera"].as_bool();
		}

		if (jsonObj.contains("AspectRatio")) {
			boost::json::object arObj = jsonObj["AspectRatio"].as_object();
			AspectRatio ar;
			ar.width = arObj["width"].as_int64();
			ar.height = arObj["height"].as_int64();
			container.aspectRatio = ar;
		}

		return container;
	}
}