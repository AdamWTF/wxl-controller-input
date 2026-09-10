#include "persistence/AtomicFile.hpp"

#include <Windows.h>

#include <fstream>

namespace wxl::controller {

bool WriteTextFileAtomically(const std::filesystem::path &path,
                             std::string_view contents) noexcept {
    try {
        const std::filesystem::path temporary = path.wstring() + L".tmp";
        bool writeSucceeded = false;
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (output) {
                output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
                output.flush();
                writeSucceeded = output.good();
            }
        }
        if (!writeSucceeded) {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            return false;
        }
        if (MoveFileExW(temporary.c_str(), path.c_str(),
                        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            return true;
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return false;
    } catch (...) {
        return false;
    }
}

} // namespace wxl::controller
