"""
################################################################################
# Script Name:  mDNS.py
# Description:  mDNS Discovery tool used to locate kprox devices on the local
#               network. Scans for all available service types and resolves
#               their IPs, hostnames, and ports to assist with debugging.
#
# Project:      kprox (ESP32-based Keyboard Proxy)
#
# Usage:        python3 discover_kprox.py
#
# Dependencies:
#   - python3
#   - zeroconf (pip install zeroconf)
#
# Safety:       Passive network scanning; does not modify device state.
#
# Environment:
#   - Works on local subnets where mDNS (Multicast DNS) is permitted.
################################################################################
"""

from zeroconf import Zeroconf, ServiceBrowser, ServiceListener
import socket
import time

# ... rest of your script

from zeroconf import Zeroconf, ServiceBrowser, ServiceListener
import socket
import time


class ServiceTypeListener(ServiceListener):
    """Collects service types."""

    def __init__(self):
        self.types = set()

    def add_service(self, zeroconf, type, name):
        self.types.add(name)


class ServiceListenerSafe(ServiceListener):
    """Safely collects individual services under each type."""

    def add_service(self, zeroconf, type, name):
        try:
            info = zeroconf.get_service_info(type, name)
            if not info:
                return

            addrs = [socket.inet_ntoa(addr) for addr in info.addresses]

            print("----- Device Found -----")
            print(f"Service: {name}")
            print(f"Type:    {type}")
            print(f"IP:      {addrs}")
            print(f"Port:    {info.port}")
            print(f"Server:  {info.server}")
            print("------------------------\n")

        except Exception:
            pass


def discover_mdns(timeout=3):
    zeroconf = Zeroconf()

    type_listener = ServiceTypeListener()
    print("Discovering service types...")
    ServiceBrowser(zeroconf, "_services._dns-sd._udp.local.", type_listener)
    time.sleep(timeout)

    print("\nScanning services under discovered types...\n")
    for service_type in list(type_listener.types):
        try:
            ServiceBrowser(zeroconf, service_type, ServiceListenerSafe())
        except Exception:
            pass

    time.sleep(timeout)
    zeroconf.close()
    print("Scan complete.")


if __name__ == "__main__":
    discover_mdns()

