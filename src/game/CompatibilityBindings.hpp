#pragma once

#include "bindings/Bindings.hpp"

#include <optional>

namespace wxl::controller {

// Resolves semantic bindings to the stock WoW 3.3.5a keyboard chords used by
// the single-DLL compatibility output path. User-provided KeyBindings pass
// through unchanged.
std::optional<KeyBinding> ResolveCompatibilityBinding(const Binding &binding) noexcept;

} // namespace wxl::controller
