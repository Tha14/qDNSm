#pragma once

#include <string>

/*
 * The DNS write path.
 *
 * Two functions, because that is all that survived the investigation. Everything else this header
 * once declared - a device-config reader, a resolver accessor, a lease reader, a periodic sampler,
 * a netctl worker thread, and a sweep across netctl_main's undocumented exports - was removed once
 * it was measured and found to be either useless or actively harmful.
 */

/*
 * Write both DNS servers through the per-field setters.
 *
 * Secondary first, then primary. Both return 0 on this hardware and read back correctly.
 */
void ApplyDnsServers(const std::string& primary, const std::string& secondary);

/*
 * Write the whole device configuration block back through the device-specific setter.
 *
 * A read-modify-write, because the block also holds the wireless passphrase, the HTTP proxy and the
 * AOSS keys. Device 0 is the live device; device 2 carries no configuration.
 *
 * Returns the setter's return code, or -1 if the block could not be read first.
 */
int WriteDeviceNetworkConfig(int device, const std::string& primary, const std::string& secondary);

/*
 * Make sure the console uses manual DNS.
 *
 * With automatic DNS the service takes its servers from DHCP and ignores the ones qDNSm just wrote,
 * so this is checked after every apply. It is read first and only written when it is actually
 * wrong. Runs on a worker thread - see the implementation for why.
 *
 * Raises a flag. Returns immediately.
 */
void RequestManualDnsFlag();

/* Starts the worker that services RequestManualDnsFlag(). Call once, at startup. */
void StartManualDnsWorker();
