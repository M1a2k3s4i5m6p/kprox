#include "registers.h"
#include "led.h"
#include "token_parser.h"

void loadRegisters() {
    if (registersLoaded) return;

    preferences.begin("kprox", false);
    activeRegister = preferences.getInt("activeReg", 0);

    int numRegisters = preferences.getInt("numRegs", 1);
    registers.clear();
    registers.resize(numRegisters);
    registerNames.clear();
    registerNames.resize(numRegisters);

    for (int i = 0; i < numRegisters; i++) {
        String key     = "reg"     + String(i);
        String nameKey = "regName" + String(i);
        registers[i]     = preferences.getString(key.c_str(),     "");
        registerNames[i] = preferences.getString(nameKey.c_str(), "");
    }

    preferences.end();
    registersLoaded = true;
}

void saveRegister(int regNum, const String& content) {
    if (regNum < 0 || (size_t)regNum >= registers.size()) return;
    preferences.begin("kprox", false);
    String key = "reg" + String(regNum);
    preferences.putString(key.c_str(), content);
    preferences.end();
    blinkLED(10, LED_COLOR_SAVE, LED_SAVE_DUTY_CYCLE);
    registers[regNum] = content;
}

void saveRegisterName(int regNum, const String& name) {
    if (regNum < 0 || (size_t)regNum >= registerNames.size()) return;
    preferences.begin("kprox", false);
    String nameKey = "regName" + String(regNum);
    preferences.putString(nameKey.c_str(), name);
    preferences.end();
    registerNames[regNum] = name;
}

void saveRegisters() {
    preferences.begin("kprox", false);
    preferences.putInt("numRegs",   registers.size());
    preferences.putInt("activeReg", activeRegister);
    for (int i = 0; i < (int)registers.size(); i++) {
        String key     = "reg"     + String(i);
        String nameKey = "regName" + String(i);
        preferences.putString(key.c_str(),     registers[i]);
        preferences.putString(nameKey.c_str(), (i < (int)registerNames.size()) ? registerNames[i] : "");
    }
    preferences.end();
    blinkLED(10, LED_COLOR_SAVE, LED_SAVE_DUTY_CYCLE);
}

void saveActiveRegister() {
    preferences.begin("kprox", false);
    preferences.putInt("activeReg", activeRegister);
    preferences.end();
    blinkLED(10, LED_COLOR_SAVE, LED_SAVE_DUTY_CYCLE);
}

void saveNumRegisters() {
    preferences.begin("kprox", false);
    preferences.putInt("numRegs", registers.size());
    preferences.end();
    blinkLED(10, LED_COLOR_SAVE, LED_SAVE_DUTY_CYCLE);
}

void addRegister(const String& content, const String& name) {
    registers.push_back(content);
    registerNames.push_back(name);
    saveNumRegisters();
    if (!content.isEmpty()) saveRegister(registers.size() - 1, content);
    if (!name.isEmpty())    saveRegisterName(registers.size() - 1, name);
}

void deleteRegister(int regNum) {
    if (regNum < 0 || (size_t)regNum >= registers.size()) return;

    if (isLooping && loopingRegister == regNum) {
        isLooping       = false;
        loopingRegister = -1;
    }

    registers.erase(registers.begin() + regNum);
    if ((size_t)regNum < registerNames.size()) {
        registerNames.erase(registerNames.begin() + regNum);
    }

    if (activeRegister >= (int)registers.size()) {
        activeRegister = max(0, (int)registers.size() - 1);
        saveActiveRegister();
    }

    preferences.begin("kprox", false);
    for (int i = 0; i < (int)registers.size(); i++) {
        String key     = "reg"     + String(i);
        String nameKey = "regName" + String(i);
        preferences.putString(key.c_str(),     registers[i]);
        preferences.putString(nameKey.c_str(), (i < (int)registerNames.size()) ? registerNames[i] : "");
    }
    String oldKey     = "reg"     + String(registers.size());
    String oldNameKey = "regName" + String(registers.size());
    preferences.remove(oldKey.c_str());
    preferences.remove(oldNameKey.c_str());
    preferences.end();

    saveNumRegisters();
}

void deleteAllRegisters() {
    isLooping       = false;
    loopingRegister = -1;

    preferences.begin("kprox", false);
    for (int i = 0; i < (int)registers.size(); i++) {
        String key = "reg" + String(i);
        preferences.remove(key.c_str());
    }
    preferences.putInt("numRegs",   0);
    preferences.putInt("activeReg", 0);
    preferences.end();

    registers.clear();
    activeRegister = 0;

    if (ledEnabled) {
        for (int i = 0; i < 5; i++) { setLED(LED_COLOR_REG_DELETE_ALL, 100); delay(100); }
    }
}

void playRegister(int regNum) {
    if (isHalted) return;
    if (regNum < 0 || (size_t)regNum >= registers.size()) return;
    if (registers[regNum].isEmpty()) return;

    if (ledEnabled) setLED(LED_COLOR_REG_PLAY, 100);
    putTokenString(registers[regNum]);
}
