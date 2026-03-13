#include "mtls.h"
#include <mbedtls/x509_crt.h>
#include "led.h"

bool   mtlsEnabled = false;
String serverCert  = "";
String serverKey   = "";
String caCert      = "";

void loadMTLSSettings() {
    Preferences prefs;
    prefs.begin(MTLS_PREF_NS, true);
    mtlsEnabled = prefs.getBool(MTLS_KEY_ENABLED, false);
    serverCert  = prefs.getString(MTLS_KEY_SERVER_CERT, "");
    serverKey   = prefs.getString(MTLS_KEY_SERVER_KEY,  "");
    caCert      = prefs.getString(MTLS_KEY_CA_CERT,     "");
    prefs.end();
}

void saveMTLSSettings() {
    Preferences prefs;
    prefs.begin(MTLS_PREF_NS, false);
    prefs.putBool(MTLS_KEY_ENABLED, mtlsEnabled);
    prefs.end();
    if (ledEnabled) blinkLED(3, LED_COLOR_SAVE, LED_SAVE_DUTY_CYCLE);
}

void saveMTLSCerts(const String& cert, const String& key, const String& ca) {
    Preferences prefs;
    prefs.begin(MTLS_PREF_NS, false);
    if (!cert.isEmpty()) { serverCert = cert; prefs.putString(MTLS_KEY_SERVER_CERT, cert); }
    if (!key.isEmpty())  { serverKey  = key;  prefs.putString(MTLS_KEY_SERVER_KEY,  key);  }
    if (!ca.isEmpty())   { caCert     = ca;   prefs.putString(MTLS_KEY_CA_CERT,     ca);   }
    prefs.end();
    if (ledEnabled) blinkLED(3, LED_COLOR_SAVE, LED_SAVE_DUTY_CYCLE);
}


bool verifyClientCert(const String& pemCert) {
    if (caCert.isEmpty() || pemCert.isEmpty()) return false;

    mbedtls_x509_crt ca, client;
    mbedtls_x509_crt_init(&ca);
    mbedtls_x509_crt_init(&client);

    bool valid = false;

    int ret = mbedtls_x509_crt_parse(&ca,
        (const unsigned char*)caCert.c_str(), caCert.length() + 1);
    if (ret == 0) {
        ret = mbedtls_x509_crt_parse(&client,
            (const unsigned char*)pemCert.c_str(), pemCert.length() + 1);
        if (ret == 0) {
            uint32_t flags = 0;
            ret = mbedtls_x509_crt_verify(&client, &ca, nullptr, nullptr, &flags, nullptr, nullptr);
            valid = (ret == 0 && flags == 0);
        }
    }

    mbedtls_x509_crt_free(&ca);
    mbedtls_x509_crt_free(&client);
    return valid;
}
