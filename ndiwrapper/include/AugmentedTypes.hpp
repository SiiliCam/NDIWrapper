#pragma once

#include "commontypes.hpp"
#include "MetaData.hpp"

template <typename T>
struct DataWithMetadata {
    T data;  // Holds the actual data of type T
    MetadataContainer metadata;  // Holds the metadata
};