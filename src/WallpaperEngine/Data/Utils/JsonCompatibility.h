#pragma once
#include <string>

namespace WallpaperEngine::Data::Utils {
// Some shipped effect definitions use trailing commas. Preserve string contents
// and line/column positions while accepting that extension at the asset boundary.
inline std::string withoutTrailingJsonCommas (std::string text) {
    bool quoted = false, escaped = false;
    for (std::size_t i = 0; i < text.size (); ++i) {
        const char c = text[i];
        if (quoted) {
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == '"') quoted = false;
        } else if (c == '"') quoted = true;
        else if (c == ',') {
            const auto next = text.find_first_not_of (" \t\r\n", i + 1);
            if (next != std::string::npos && (text[next] == ']' || text[next] == '}')) text[i] = ' ';
        }
    }
    return text;
}
}
