#pragma once

#include <QString>

namespace CFDojo {

inline constexpr auto kVersion = "0.2.0";

inline QString versionString() {
    return QStringLiteral("v") + QString::fromLatin1(kVersion);
}

} // namespace CFDojo
