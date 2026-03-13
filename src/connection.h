#pragma once

#include "globals.h"

void enableBluetooth();
void disableBluetooth();

void enableUSB();
void disableUSB();

bool connectToNewWiFi(const String& newSSID, const String& newPassword);
void setupMDNS();
void cleanupConnections();
void initNTP();
void broadcastDiscovery();
