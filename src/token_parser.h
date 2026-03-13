#pragma once

#include "globals.h"
#include <map>

void putTokenString(const String& text);
void parseAndSendText(const String& text);
void parseAndSendText(const String& text, std::map<String, String>& vars);
