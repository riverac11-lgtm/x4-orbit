#pragma once

#include <string>

// How the Library writes a folder's name on its row. Presentation only: the entry name and every
// path built from it keep the card's bytes (FAT lookup is byte-exact), and nothing is renamed.
//
// The name is shown as it is, without the "[...]" the browser used to wrap it in -- the row's
// trailing marker (BaseTheme::submenuMarker) now says "this opens", as it does on a settings row --
// except that a name written all in capitals, as the reader's own /COMPLETED folder is, is shown
// with only its first letters capital ("Completed"): shouting is not what the folder means.
inline std::string folderDisplayName(std::string name) {
  bool hasUpper = false;
  for (const char c : name) {
    if (c >= 'a' && c <= 'z') return name;                   // the owner's own casing: keep it
    if (static_cast<unsigned char>(c) >= 0x80) return name;  // not plain ASCII: leave it alone
    if (c >= 'A' && c <= 'Z') hasUpper = true;
  }
  if (!hasUpper || name.size() < 2) return name;
  bool wordStart = true;
  for (char& c : name) {
    if (c >= 'A' && c <= 'Z' && !wordStart) c = static_cast<char>(c - 'A' + 'a');
    wordStart = !((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'));
  }
  return name;
}
