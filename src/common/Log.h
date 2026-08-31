// EXT-17 — logging. Never throws (constraint C3); every failure elsewhere is a return value,
// and this is where the accompanying line goes.
//
// A log line carries a stage tag and a message. Nothing here reads a wall clock: the campaign's
// evidence is keyed on simulation quantities and on ordinals, and a timestamp in a file that a
// later milestone might compare is exactly the hazard [B] names first.
#pragma once

#include <string>

namespace ext17::log {

// Mirror every subsequent line into this file as well as stdout. Returns false if the file
// could not be opened; logging then continues to stdout alone.
bool mirrorToFile(const std::string& path);
void closeMirror();

void line(const char* stage, const std::string& text);

} // namespace ext17::log
