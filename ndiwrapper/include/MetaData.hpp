#pragma once
#include <string>
#include <optional>
#include <functional>
#include "tinyxml2.h"

/**
 * @brief contains some application specific metadata handlers, currently only hard coded structs
 */
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


using MetaDataCallback = std::function<void(MetadataContainer)>;

namespace Metadata {
    template<typename T>
    inline void addToXML(tinyxml2::XMLDocument& doc, tinyxml2::XMLElement* root, const T& data);

    template<>
    inline void addToXML<BoundingBox>(tinyxml2::XMLDocument& doc, tinyxml2::XMLElement* root, const BoundingBox& box) {
        auto element = doc.NewElement("BoundingBox");
        auto start = doc.NewElement("start");
        start->SetAttribute("x", box.start.x);
        start->SetAttribute("y", box.start.y);
        element->InsertEndChild(start);
        element->SetAttribute("width", box.width);
        element->SetAttribute("height", box.height);
        root->InsertEndChild(element);
    }

    template<>
    inline void addToXML<Zoom>(tinyxml2::XMLDocument& doc, tinyxml2::XMLElement* root, const Zoom& zoom) {
        auto element = doc.NewElement("Zoom");
        element->SetText(zoom);
        root->InsertEndChild(element);
    }

    template<>
    inline void addToXML<SwitchCamera>(tinyxml2::XMLDocument& doc, tinyxml2::XMLElement* root, const SwitchCamera& switchCamera) {
        auto element = doc.NewElement("SwitchCamera");
        element->SetText(switchCamera);
        root->InsertEndChild(element);
    }

    template<>
    inline void addToXML<AspectRatio>(tinyxml2::XMLDocument& doc, tinyxml2::XMLElement* root, const AspectRatio& aspectRatio) {
        auto element = doc.NewElement("AspectRatio");
        element->SetAttribute("width", aspectRatio.width);
        element->SetAttribute("height", aspectRatio.height);
        root->InsertEndChild(element);
    }

    template<typename... Args>
    inline std::string encode(const Args&... args) {
        tinyxml2::XMLDocument doc;
        auto declaration = doc.NewDeclaration();
        doc.InsertFirstChild(declaration);
        auto root = doc.NewElement("root");
        doc.InsertEndChild(root);

        (addToXML(doc, root, args), ...);

        tinyxml2::XMLPrinter printer;
        doc.Print(&printer);
        return printer.CStr();
    }

    inline MetadataContainer decode(const std::string& xmlStr) {
        MetadataContainer container;
        tinyxml2::XMLDocument doc;
        doc.Parse(xmlStr.c_str());

        auto root = doc.FirstChildElement("root");
        if (root) {
            auto bboxElement = root->FirstChildElement("BoundingBox");
            if (bboxElement) {
                BoundingBox box;
                auto start = bboxElement->FirstChildElement("start");
                box.start.x = start->IntAttribute("x");
                box.start.y = start->IntAttribute("y");
                box.width = bboxElement->IntAttribute("width");
                box.height = bboxElement->IntAttribute("height");
                container.boundingBox = box;
            }

            auto zoomElement = root->FirstChildElement("Zoom");
            if (zoomElement) {
                container.zoom = zoomElement->DoubleText();
            }

            auto switchCameraElement = root->FirstChildElement("SwitchCamera");
            if (switchCameraElement) {
                container.switchCamera = switchCameraElement->BoolText();
            }

            auto aspectRatioElement = root->FirstChildElement("AspectRatio");
            if (aspectRatioElement) {
                AspectRatio ar;
                ar.width = aspectRatioElement->IntAttribute("width");
                ar.height = aspectRatioElement->IntAttribute("height");
                container.aspectRatio = ar;
            }
        }

        return container;
    }
}