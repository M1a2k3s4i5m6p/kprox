#include "keymap.h"
#include "keymap_data.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>

String activeKeymap = "en";

static std::map<uint16_t, KeyEntry> _table;
static String _loadedId = "";

static uint16_t utf8ToCodepoint(const char* s, int& bytesConsumed) {
    uint8_t b = (uint8_t)s[0];
    if (b < 0x80)           { bytesConsumed = 1; return b; }
    if ((b & 0xE0) == 0xC0) { bytesConsumed = 2; return ((b & 0x1F) << 6) | ((uint8_t)s[1] & 0x3F); }
    if ((b & 0xF0) == 0xE0) { bytesConsumed = 3; return ((b & 0x0F) << 12) | (((uint8_t)s[1] & 0x3F) << 6) | ((uint8_t)s[2] & 0x3F); }
    bytesConsumed = 1;
    return 0;
}

static uint16_t singleCharCodepoint(const char* s) {
    int n;
    return utf8ToCodepoint(s, n);
}

static bool parseAndLoadJson(const String& json) {
    JsonDocument doc;
    if (deserializeJson(doc, json) != DeserializationError::Ok) return false;
    _table.clear();
    JsonArray arr = doc["map"].as<JsonArray>();
    for (JsonObject entry : arr) {
        const char* ch = entry["char"];
        if (!ch || ch[0] == '\0') continue;
        uint16_t cp = singleCharCodepoint(ch);
        if (cp == 0) continue;
        KeyEntry ke;
        ke.hidUsage  = entry["key"].as<uint8_t>();
        ke.modifiers = entry["mod"].as<uint8_t>();
        _table[cp]   = ke;
    }
    return true;
}

static void provisionBuiltinKeymaps() {
    for (int i = 0; i < BUILTIN_KEYMAP_COUNT; i++) {
        String path = "/keymaps/";
        path += BUILTIN_KEYMAPS[i].id;
        path += ".json";
        if (!SPIFFS.exists(path)) {
            File f = SPIFFS.open(path, "w");
            if (f) {
                f.print(BUILTIN_KEYMAPS[i].data);
                f.close();
            }
        }
    }
}

void keymapInit() {
    provisionBuiltinKeymaps();
    preferences.begin("kprox", true);
    activeKeymap = preferences.getString("keymap", "en");
    preferences.end();

    if (activeKeymap != "en") {
        String path = "/keymaps/" + activeKeymap + ".json";
        if (!SPIFFS.exists(path)) {
            activeKeymap = "en";
        } else {
            File f = SPIFFS.open(path, "r");
            if (f) {
                String json = f.readString();
                f.close();
                if (!parseAndLoadJson(json)) {
                    activeKeymap = "en";
                    _table.clear();
                }
                _loadedId = activeKeymap;
            }
        }
    }
}

bool keymapLoad(const String& id) {
    if (id == "en") {
        _table.clear();
        _loadedId    = "en";
        activeKeymap = "en";
        return true;
    }

    String path = "/keymaps/" + id + ".json";
    if (!SPIFFS.exists(path)) return false;

    File f = SPIFFS.open(path, "r");
    if (!f) return false;
    String json = f.readString();
    f.close();

    if (!parseAndLoadJson(json)) return false;

    _loadedId    = id;
    activeKeymap = id;
    return true;
}

bool keymapExists(const String& id) {
    if (id == "en") return true;
    return SPIFFS.exists("/keymaps/" + id + ".json");
}

bool keymapLookup(uint16_t codepoint, KeyEntry& out) {
    auto it = _table.find(codepoint);
    if (it == _table.end()) return false;
    out = it->second;
    return true;
}

void keymapSaveActive() {
    preferences.begin("kprox", false);
    preferences.putString("keymap", activeKeymap);
    preferences.end();
}

String keymapActive() {
    return activeKeymap;
}

std::vector<String> keymapListAvailable() {
    std::vector<String> list;
    list.push_back("en");

    File root = SPIFFS.open("/keymaps");
    if (root && root.isDirectory()) {
        File f = root.openNextFile();
        while (f) {
            String name = String(f.name());
            // SPIFFS returns full path on some versions, just filename on others
            int slash = name.lastIndexOf('/');
            if (slash >= 0) name = name.substring(slash + 1);
            if (name.endsWith(".json")) {
                String id = name.substring(0, name.length() - 5);
                if (id != "en") list.push_back(id);
            }
            f = root.openNextFile();
        }
    }
    return list;
}

bool keymapDelete(const String& id) {
    if (id == "en") return false; // built-in, cannot delete
    String path = "/keymaps/" + id + ".json";
    if (!SPIFFS.exists(path)) return false;
    bool ok = SPIFFS.remove(path);
    if (ok && activeKeymap == id) {
        activeKeymap = "en";
        _table.clear();
        _loadedId    = "en";
        keymapSaveActive();
    }
    return ok;
}

bool keymapUpload(const String& id, const String& json) {
    if (id.isEmpty() || id == "en") return false;
    // Validate JSON before writing
    JsonDocument doc;
    if (deserializeJson(doc, json) != DeserializationError::Ok) return false;
    if (!doc["map"].is<JsonArray>()) return false;

    String path = "/keymaps/" + id + ".json";
    File f = SPIFFS.open(path, "w");
    if (!f) return false;
    f.print(json);
    f.close();
    return true;
}
