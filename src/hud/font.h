#pragma once

// The fallback face, compiled in so the HUD has correct proportions on a machine with no
// condensed font installed, and so the plugin never has to fetch anything. See font.cpp for what
// it is and where it came from; tools/embed_font.py regenerates it from the file in font/.
namespace hud {

extern const char* const kFallbackFontMime;
extern const char* const kFallbackFontFormat;
extern const char* const kFallbackFontWeights;

// Base64, split into pieces so no single string literal approaches MSVC's 65,535 byte cap.
// Null terminated.
extern const char* const kFallbackFontBase64Parts[];

}  // namespace hud
