# KProx Control Android App

This is a full Android control app for the KProx firmware that provides
device discovery using mDNS and most of the features provided by the KProx web 
app.


## How to Build:
With GNU Make and Docker installed run: 
```bash
make build
```
and after connecting and Android device and granting adb access run (requires adb to be installed locally):
```
make install
```

## Project Structure:
- `MainActivity.java` - Main Android activity with device discovery and text sending
- `discovery.html` - Web interface with tab/button management system
- `AndroidManifest.xml` - App configuration with required permissions

## Usage:
1. App auto-discovers KProx devices on startup
2. Tap a device to select it (indicator turns green)
3. Create tabs to organize your buttons
4. Add buttons with custom labels and text commands
5. Tap buttons to send text to the selected device
