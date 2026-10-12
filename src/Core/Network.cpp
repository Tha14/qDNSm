#include "Network.hpp"

#include <sys/ppu_thread.h>

/*
 * xregistry.h carries no linkage guard of its own, so every consumer wraps it. Skipping the wrap
 * gave these declarations C++ linkage and the link failed with _Z17xsetting_F48C0548v - the
 * mangled form of xsetting_F48C0548(), against a stub library that exports it unmangled.
 *
 * Matches the pattern already used in Submenus.cpp and Submenus.hpp.
 */
extern "C" {
	#include "../Utils/xregistry.h"   // address, net_info, xnet
}

#include "../Utils/Timers.hpp"   // Sleep
#include <cstring>

/*
 * The DNS write path, and nothing else.
 *
 * This file used to carry about 900 lines of investigation machinery: a diagnostic logger, a
 * periodic sampler, detours on netctl_main and sys_net, a worker thread that drove the connection
 * through a disconnect and reconnect, and a sweep that called every undocumented export in
 * netctl_main to see what would happen. All of it is gone.
 *
 * What survived is the part that works and the part that is honest.
 *
 * What works: the servers are stored correctly, verified by readback on hardware many times over.
 * Both the per-field setters and the device-specific aggregate write land in the console's own
 * configuration and can be read straight back out of it.
 *
 * What does not work: making the console act on the change. The console reads its DNS settings when
 * it builds a connection, and nothing reachable from a third-party SPRX can make it do that. The
 * XMB's own "Test Network Connection" does, and the user has to press it.
 *
 * Everything that was tried is written up in TODO.md, including the two things that cost this
 * project a console reboot each. The short version, so nobody re-treads it:
 *
 *   - netctl_main exports must be called through their linked stubs. opd_s is {func, toc}, and
 *     casting func and calling it produced calls that neither faulted nor returned, which read as a
 *     wall in the network service and was entirely our own bug.
 *
 *   - sceNetApCtlTermVsh returns success while terminating a network stack this module does not
 *     own, and the console then stops assigning DHCP addresses until it is rebooted. It was tried
 *     deliberately, once, and confirmed again that it cannot be undone from here. Do not re-add it.
 */

/*
 * Builds the 16-byte address struct the xnet setters expect.
 *
 * Copied through an explicitly bounded loop rather than std::copy or a cast, and this is not
 * stylistic: the obvious version of this function used value.data() as a null-terminated buffer,
 * which is only guaranteed from C++17 onwards and is not guaranteed at all on this toolchain. The
 * setters read the full 16 bytes, so a missing terminator is a real read past the end of a
 * std::string's buffer rather than a theoretical one.
 *
 * "255.255.255.255" is 15 characters plus a terminator, which is exactly 16, so the bound is size-1
 * and the loop always leaves room to terminate.
 */
static address MakeAddress(const std::string& text)
{
	address result{};

	std::size_t i = 0;

	while ((i < text.size()) && (i < (sizeof(result.addr) - 1)))
	{
		result.addr[i] = text[i];
		i++;
	}

	result.addr[i] = '\0';
	return result;
}

/*
 * Copies a string into one of the configuration block's fixed-size char fields.
 *
 * Zeroed first rather than terminated in place, so a shorter new value cannot leave the tail of a
 * longer old one behind. primaryDns[0x10] is exactly 16 bytes and the longest realistic address is
 * 15 characters, so the copy is bounded at size-1 and never runs off the end.
 */
static void FieldFromString(char* data, std::size_t size, const std::string& text)
{
	std::memset(data, 0, size);

	std::size_t i = 0;

	while ((i < (size - 1)) && (i < text.size()))
	{
		data[i] = text[i];
		i++;
	}
}

/*
 * Write both DNS servers through the per-field setters.
 *
 * Secondary first. The order is not cosmetic: a console that is mid-reconnect reads these in
 * sequence, and writing the primary first would leave a window where the new primary is paired with
 * the old secondary.
 *
 * Returns nothing. The return codes of these setters were logged on hardware many times and were
 * always 0, and the readback confirmed the values landed every time, so there is nothing here for a
 * caller to act on.
 */
void ApplyDnsServers(const std::string& primary, const std::string& secondary)
{
	if (!secondary.empty())
	{
		address secondaryAddress = MakeAddress(secondary);

		xnet()->SetSettingNet_secondaryDns(&secondaryAddress);
	}

	address primaryAddress = MakeAddress(primary);

	xnet()->SetSettingNet_primaryDns(&primaryAddress);
}

/*
 * Write the whole device configuration block back through the device-specific setter.
 *
 * This is the only documented call that touches the entire configuration at once, so it is the one
 * most likely to carry the validation and notification that a single-field write skips. It is a
 * read-modify-write for that reason: the block also holds the wireless passphrase, the HTTP proxy
 * and the AOSS keys, and writing a partially populated struct would blank all three.
 *
 * Device 0 is the live one. Device 2 was probed and came back completely empty on hardware, so it
 * is not carrying a configuration at all.
 *
 * Returns the setter's return code, or -1 if the block could not be read in the first place.
 */
int WriteDeviceNetworkConfig(int device, const std::string& primary, const std::string& secondary)
{
	net_info config;

	std::memset(&config, 0, sizeof(config));

	/* Read first, through the same accessor the write uses. The plain getter takes only the struct
	 * and addresses a different slot, so mixing the two is how a correct write ends up looking like
	 * it persisted nothing. */
	if (static_cast<int>(xnet()->sub_44AA24(device, &config)) != 0)
		return -1;

	FieldFromString(config.primaryDns, sizeof(config.primaryDns), primary);
	FieldFromString(config.secondaryDns, sizeof(config.secondaryDns),
		secondary.empty() ? std::string("0.0.0.0") : secondary);

	return static_cast<int>(xnet()->sub_44B2EC(device, &config));
}

/*
 * Make sure the console is set to use the DNS servers qDNSm just wrote.
 *
 * This is the one setting that silently decides whether any of it matters. With automatic DNS the
 * service takes its servers from DHCP and primaryDns/secondaryDns are never consulted, so a user who
 * switched DNS on the XMB years ago and forgot sees qDNSm report success and see no change at all.
 * It is the single most likely reason for "it doesn't work", and it costs one flag to rule out.
 *
 * It is read first and only written when it is actually wrong, because this is a stored setting and
 * an unnecessary write is a change the user did not ask for.
 *
 * It runs on a worker thread. The original author left this call commented out with the note
 * "Causes XMB to freeze", and while that is most likely about doing it from the frame hook rather
 * than about the call itself, the cheap answer is to not do it there. Nothing on the XMB's frame
 * loop should ever be able to freeze the XMB's frame loop.
 */
static volatile bool s_ManualDnsRequested = false;

void RequestManualDnsFlag()
{
	s_ManualDnsRequested = true;
}

static void EnsureManualDns()
{
	int flag = -1;

	if (xnet()->GetSettingNet_dnsFlag(&flag) != 0)
		return;

	/* Already manual. Leave the setting alone. */
	if (flag != 0)
		return;

	xnet()->SetSettingNet_dnsFlag(1);
}

static void ManualDnsWorker(uint64_t arg)
{
	(void)arg;

	/* Poll rather than block, so the request is never anything more than a flag store on the
	 * caller's thread. The apply path is the XMB's frame loop. */
	while (true)
	{
		if (s_ManualDnsRequested)
		{
			s_ManualDnsRequested = false;

			EnsureManualDns();
		}

		Sleep(250);
	}
}

void StartManualDnsWorker()
{
	sys_ppu_thread_t workerThreadId = SYS_PPU_THREAD_ID_INVALID;

	/* Argument order is (id, entry, arg, priority, stackSize, flags, name) and the valid priority
	 * range is 0-3071. An earlier version of this project passed 16384 as the priority, so the
	 * thread was silently never created and nothing happened with no error at all. */
	sys_ppu_thread_create(&workerThreadId, [](uint64_t arg) -> void
	{
		ManualDnsWorker(arg);
		sys_ppu_thread_exit(0);
	}, 0, 1000, 4096, SYS_PPU_THREAD_CREATE_JOINABLE, "qDNSmDns");
}
