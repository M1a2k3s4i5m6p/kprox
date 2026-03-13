#pragma once

#include "globals.h"

// Certificate storage keys in Preferences
#define MTLS_PREF_NS        "kprox_tls"
#define MTLS_KEY_SERVER_CERT "srv_cert"
#define MTLS_KEY_SERVER_KEY  "srv_key"
#define MTLS_KEY_CA_CERT     "ca_cert"
#define MTLS_KEY_ENABLED     "enabled"

// Runtime TLS state
extern bool   mtlsEnabled;
extern String serverCert;
extern String serverKey;
extern String caCert;

void loadMTLSSettings();
void saveMTLSSettings();
void saveMTLSCerts(const String& cert, const String& key, const String& ca);

bool verifyClientCert(const String& pemCert);
