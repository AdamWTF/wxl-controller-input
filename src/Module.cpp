#include "ExtensionApi.hpp"
#include "wxl/PluginApi.h"

extern "C" {

__declspec(dllexport) const WXL_PluginInfo *__cdecl WXL_Query(void) {
    static constexpr WXL_PluginInfo info{sizeof(WXL_PluginInfo), WXL_API_VERSION,
                                         "WarcraftXL Controller Input", 0x000100, WXL_CLIENT_BUILD};
    return &info;
}

__declspec(dllexport) int __cdecl WXL_Load(const WXL_Api *api) {
    return wxl::controller::LoadExtension(api) ? 1 : 0;
}
}
