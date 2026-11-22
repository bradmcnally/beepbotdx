#pragma once

#include <filesystem>

namespace Storage {
const std::filesystem::path& dataDirectory();
std::filesystem::path sampleDirectory();
}
