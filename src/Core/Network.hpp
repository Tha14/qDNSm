#pragma once

#include <string>

/*
 * Write both DNS servers through the per-field setters.
 *
 * This is the keys write that was proven on hardware. It updates the xsetting keys the
 * config connect builds from, and nothing else.
 */
void ApplyDnsServers(const std::string& primary, const std::string& secondary);

/*
 * Write the whole device configuration block back through the device-specific setter.
 *
 * This is the syscall pair the non-auto path used: read the live device configuration,
 * change the two DNS fields, set the manual flag in the block, and write it back. It is
 * what updates the configuration the XMB's network status list displays; the per-field
 * setters alone update the keys, not that list.
 *
 * The manual flag is part of the block on purpose. Writing it with the standalone
 * setter (SetSettingNet_dnsFlag) is what hangs the console; the block write carries the
 * flag without that call.
 *
 * Device 0 is the live device. Returns the setter's return code, or -1 if the block
 * could not be read first.
 */
int WriteDeviceNetworkConfig(int device, const std::string& primary, const std::string& secondary);

/*
 * Ask the worker to take the connection down and bring it back up with the servers that
 * were just written. The server's name is kept for the completion notification.
 */
void RequestReconnect(const std::string& serverName);

/* Starts the worker that services RequestReconnect(). Call once, at startup. */
void StartManualDnsWorker();
