#pragma once

#include <filesystem>
#include <string_view>

namespace wxl::controller {

bool WriteTextFileAtomically(const std::filesystem::path &path, std::string_view contents) noexcept;

} // namespace wxl::controller
