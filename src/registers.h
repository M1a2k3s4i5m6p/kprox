#pragma once

#include "globals.h"

void loadRegisters();
void saveRegister(int regNum, const String& content);
void saveRegisterName(int regNum, const String& name);
void saveRegisters();
void saveActiveRegister();
void saveNumRegisters();

void addRegister(const String& content = "", const String& name = "");
void deleteRegister(int regNum);
void deleteAllRegisters();
void playRegister(int regNum);
