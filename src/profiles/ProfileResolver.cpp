#include "profiles/ProfileResolver.hpp"

namespace wxl::controller {

ProfileResolver::ProfileResolver() : builtIn_(BuiltInBindings()) {}

std::optional<ResolvedBinding> ProfileResolver::Resolve(BindingKey key) const {
    if (identity_) if (const auto it = character_.find(key); it != character_.end())
        return ResolvedBinding{it->second, BindingSource::Character};
    if (const auto it = global_.find(key); it != global_.end())
        return ResolvedBinding{it->second, BindingSource::Global};
    if (const auto it = builtIn_.find(key); it != builtIn_.end())
        return ResolvedBinding{it->second, BindingSource::BuiltIn};
    return std::nullopt;
}

} // namespace wxl::controller

