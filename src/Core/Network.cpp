#include "Network.hpp"
#include "Config.hpp"   // AsciiToWide

#include <sys/ppu_thread.h>

extern "C" {
	#include "../Utils/xregistry.h"   // address, xnet
}

#include "../Utils/Timers.hpp"   // Sleep
#include "Helpers.hpp"
#include <vsh/vshmain.hpp>
#include <vsh/stdc.hpp>              // memset, strncpy
#include <cstring>

/*
 * The VSH side netctl exports.
 *
 * These are the linked stubs from netctl_main_export_stub. They must be called through
 * the stub: building an OPD by hand and casting it produced a call that neither faulted
 * nor returned.
 */
extern "C" int netctl_main_218CFBD3(void* threadInfo, int priority, int stackSize); // sceNetCtlInitVsh
extern "C" int netctl_main_EC73B49D(int* state);   // sceNetCtlGetStateVsh
extern "C" int netctl_main_5AB91391(void* threadInfo); // sceNetCtlDisconnectVsh
extern "C" int netctl_main_5D1D7803(void* config); // sceNetCtlConnectVsh

static const uint32_t kConnectConfigSize = 0x68C;

static uint8_t s_PendingConnectConfig[kConnectConfigSize];
static bool s_PendingConnectConfigReady = false;

/* The waits, in 250 ms polls. */
static const int kDownWaitTicks = 36;   /* 9 seconds */
static const int kUpWaitTicks = 40;     /* 10 seconds */

static std::string BoundedString(const char* data, std::size_t size)
{
	std::size_t length = 0;

	while ((length < size) && (data[length] != '\0'))
		length++;

	return std::string(data, length);
}

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
 * Zeroed first rather than terminated in place, so a shorter new value cannot leave the
 * tail of a longer old one behind. primaryDns[0x10] is exactly 16 bytes and the longest
 * realistic address is 15 characters, so the copy is bounded at size-1.
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

static void BuildConnectConfig(const net_info& info, uint8_t* config)
{
	std::memset(config, 0, kConnectConfigSize);

	std::memcpy(config + 0x000, &info.device, sizeof(info.device));
	std::memcpy(config + 0x004, &info.mtu, sizeof(info.mtu));
	std::memcpy(config + 0x008, &info.etherMode, sizeof(info.etherMode));
	std::memcpy(config + 0x00C, info.ssid, sizeof(info.ssid));
	std::memcpy(config + 0x030, &info.authProto, sizeof(info.authProto));
	std::memcpy(config + 0x034, info.wirelessPassPhrase, sizeof(info.wirelessPassPhrase));
	std::memcpy(config + 0x078, &info.auth8021xType, sizeof(info.auth8021xType));
	std::memcpy(config + 0x07C, info.auth8021xAuthName, sizeof(info.auth8021xAuthName));
	std::memcpy(config + 0x0FC, info.auth8021xAuthKey, sizeof(info.auth8021xAuthKey));
	std::memcpy(config + 0x180, &info.howToSetupIp, sizeof(info.howToSetupIp));
	std::memcpy(config + 0x184, info.dhcpHostName, sizeof(info.dhcpHostName));
	std::memcpy(config + 0x283, info.authName, sizeof(info.authName));
	std::memcpy(config + 0x303, info.authKey, sizeof(info.authKey));
	std::memcpy(config + 0x383, info.ipAddress, sizeof(info.ipAddress));
	std::memcpy(config + 0x393, info.netmask, sizeof(info.netmask));
	std::memcpy(config + 0x3A3, info.defaultRoute, sizeof(info.defaultRoute));
	std::memcpy(config + 0x3B4, &info.dnsFlag, sizeof(info.dnsFlag));
	std::memcpy(config + 0x3B8, info.primaryDns, sizeof(info.primaryDns));
	std::memcpy(config + 0x3C8, info.secondaryDns, sizeof(info.secondaryDns));
	std::memcpy(config + 0x3D8, &info.httpProxyFlag, sizeof(info.httpProxyFlag));
	std::memcpy(config + 0x3DC, info.httpProxyServer, sizeof(info.httpProxyServer));
	std::memcpy(config + 0x4DC, &info.httpProxyPort, sizeof(info.httpProxyPort));

	int32_t upnpFlag = info.upnpFlag;

	if (upnpFlag == 0)
		upnpFlag = 1;
	else if (upnpFlag == 1)
		upnpFlag = 0;

	std::memcpy(config + 0x4E0, &upnpFlag, sizeof(upnpFlag));
	std::memcpy(config + 0x4E4, &info.aossFlag, sizeof(info.aossFlag));

	std::memcpy(config + 0x4E8, info.aossWep64Ssid, sizeof(info.aossWep64Ssid));
	std::memcpy(config + 0x50C, info.aossWep64Key, sizeof(info.aossWep64Key));
	std::memcpy(config + 0x54D, info.aossWep128Ssid, sizeof(info.aossWep128Ssid));
	std::memcpy(config + 0x571, info.aossWep128Key, sizeof(info.aossWep128Key));
	std::memcpy(config + 0x5B2, info.aossWpaPskTkipSsid, sizeof(info.aossWpaPskTkipSsid));
	std::memcpy(config + 0x5D6, info.aossWpaPskTkipKey, sizeof(info.aossWpaPskTkipKey));
	std::memcpy(config + 0x617, info.aossWpaPskAesSsid, sizeof(info.aossWpaPskAesSsid));
	std::memcpy(config + 0x63B, info.aossWpaPskAesKey, sizeof(info.aossWpaPskAesKey));
}

namespace Netctl
{
	static uint8_t s_ThreadInfo[0x200];

	int Init()
	{
		vsh::memset(s_ThreadInfo, 0, sizeof(s_ThreadInfo));
		int result = netctl_main_218CFBD3(&s_ThreadInfo[0], 1000, 0x10000);
		return result;
	}

	int GetState()
	{
		int state = -1;
		int result = netctl_main_EC73B49D(&state);
		return state;
	}

	int Disconnect()
	{
		int result = netctl_main_5AB91391(&s_ThreadInfo[0]);
		return result;
	}


	int ConnectWithConfig(uint8_t* config)
	{
		return netctl_main_5D1D7803(config);
	}
}

/*
 * The two servers in the keys the config connect builds from. The manual flag is not
 * written here: it travels in the device block.
 */
void ApplyDnsServers(const std::string& primary, const std::string& secondary)
{
	/*
	 * Read before writing: a value that is already there is not written again. Writing a
	 * setting tells the console's service to read its configuration again, and a write
	 * that changes nothing makes it do that for nothing.
	 */
	address current{};
	address value{};

	int primaryGet = xnet()->GetSettingNet_primaryDns(&current);
	std::string currentPrimary = BoundedString(current.addr, sizeof(current.addr));

	if ((primaryGet != 0) || (currentPrimary != primary))
	{
		value = MakeAddress(primary);
		xnet()->SetSettingNet_primaryDns(&value);
	}

	current = address{};

	int secondaryGet = xnet()->GetSettingNet_secondaryDns(&current);
	std::string currentSecondary = BoundedString(current.addr, sizeof(current.addr));

	if ((secondaryGet != 0) || (currentSecondary != secondary))
	{
		value = MakeAddress(secondary);
		xnet()->SetSettingNet_secondaryDns(&value);
	}
}

/*
 * The device block write, the one the non-auto path used.
 *
 * Read the live device configuration, change the two DNS fields, set the manual flag in
 * the block, and write it back. This is what updates the configuration the XMB's
 * network status list displays; the per-field setters above update the keys, not that
 * list.
 *
 * The flag is written as part of the block. The standalone flag setter
 * (SetSettingNet_dnsFlag) is the call that hangs the console, so it is not used.
 *
 * Device 0 is the live device. Returns the setter's return code, or -1 if the block
 * could not be read first.
 */
int WriteDeviceNetworkConfig(int device, const std::string& primary, const std::string& secondary)
{
	net_info config;

	std::memset(&config, 0, sizeof(config));

	int read = static_cast<int>(xnet()->sub_44AA24(device, &config));

	if (read != 0)
		return -1;

	FieldFromString(config.primaryDns, sizeof(config.primaryDns), primary);
	FieldFromString(config.secondaryDns, sizeof(config.secondaryDns),
		secondary.empty() ? std::string("0.0.0.0") : secondary);

	/* Manual DNS. Carried by the block so the standalone flag setter is not called. */
	config.dnsFlag = 1;

	int write = static_cast<int>(xnet()->sub_44B2EC(device, &config));

	if (write == 0)
	{
		BuildConnectConfig(config, s_PendingConnectConfig);
		s_PendingConnectConfigReady = true;
	}
	else
	{
		s_PendingConnectConfigReady = false;
	}

	return write;
}

static volatile bool s_ReconnectRequested = false;
static volatile bool s_ReconnectRunning = false;
static volatile int s_DnsApplyPhase = DnsApply_Idle;
static bool s_NetctlInitialized = false;
static char s_ReconnectName[64] = {};

bool IsDnsApplyBusy()
{
	return (s_DnsApplyPhase != DnsApply_Idle) || s_ReconnectRunning;
}

bool BeginDnsApply()
{
	if (IsDnsApplyBusy())
		return false;

	s_DnsApplyPhase = DnsApply_Applying;
	return true;
}

void EndDnsApply()
{
	s_DnsApplyPhase = DnsApply_Idle;
}

DnsApplyPhase GetDnsApplyPhase()
{
	return static_cast<DnsApplyPhase>(s_DnsApplyPhase);
}

static void ShowReconnectFailure(bool willFail)
{

	std::string message = willFail
		? "Automatic reconnect will fail."
		: "Automatic reconnect failed.";
	message += "\nDisable it from the qDNSm options menu.";

	std::wstring wide;

	AsciiToWide(wide, message);

	vsh::ShowNotificationWithIcon(wide, vsh::NotifyIcon::Caution,
		vsh::NotifySound::Error);
}

static void NetworkThread(uint64_t)
{
	uint8_t config[kConnectConfigSize];

	if (!s_PendingConnectConfigReady)
	{
		ShowReconnectFailure(true);
		s_ReconnectRunning = false;
		s_DnsApplyPhase = DnsApply_Idle;
		sys_ppu_thread_exit(0);
	}

	if (!s_NetctlInitialized)
	{
		Netctl::Init();
		s_NetctlInitialized = true;
	}

	std::memcpy(config, s_PendingConnectConfig, sizeof(config));

	Netctl::Disconnect();

	int state = Netctl::GetState();
	int downTicks = 0;

	while ((state != 0) && (downTicks < kDownWaitTicks))
	{
		Sleep(250);
		downTicks++;
		state = Netctl::GetState();
	}

	Netctl::ConnectWithConfig(config);

	int upTicks = 0;

	while (((state != 4) && (state != 5)) && (upTicks < kUpWaitTicks))
	{
		Sleep(250);
		upTicks++;
		state = Netctl::GetState();
	}

	if ((state == 4) || (state == 5))
	{
		if (s_ReconnectName[0] != '\0')
		{

			std::string message = "DNS was changed to ";
			message += s_ReconnectName;
			message += ".";

			std::wstring wide;

			AsciiToWide(wide, message);

			vsh::ShowNotificationWithIcon(wide, vsh::NotifyIcon::BlueVerifiedCheckmark,
				vsh::NotifySound::OK);
		}
		else
		{
		}
	}
	else
	{
		ShowReconnectFailure(false);
	}

	s_ReconnectRunning = false;
	s_DnsApplyPhase = DnsApply_Idle;
	sys_ppu_thread_exit(0);
}

static void ReconnectStep()
{
	if (!s_ReconnectRequested)
		return;

	s_ReconnectRequested = false;

	sys_ppu_thread_t threadId = SYS_PPU_THREAD_ID_INVALID;

	/*
	 * 64 KB: this thread also builds the configuration and runs the connect, which is
	 * the stack size the config connect was proven with on hardware.
	 */
	int createResult = sys_ppu_thread_create(&threadId, [](uint64_t arg) -> void
	{
		NetworkThread(arg);
	}, 0, 1000, 0x10000, SYS_PPU_THREAD_CREATE_JOINABLE, "qDNSmNet");

	if (createResult != 0)
	{
		s_ReconnectRunning = false;
		s_DnsApplyPhase = DnsApply_Idle;
		ShowReconnectFailure(false);
	}
}

bool RequestReconnect(const std::string& serverName)
{

	if (s_ReconnectRunning || !s_PendingConnectConfigReady)
	{
		return false;
	}

	vsh::memset(s_ReconnectName, 0, sizeof(s_ReconnectName));
	vsh::strncpy(s_ReconnectName, serverName.c_str(), sizeof(s_ReconnectName) - 1);

	s_ReconnectRunning = true;
	s_ReconnectRequested = true;
	s_DnsApplyPhase = DnsApply_Reconnecting;
	return true;
}

static void ManualDnsWorker(uint64_t)
{
	int ticks = 0;

	while (true)
	{
		ReconnectStep();

		ticks++;
		if ((ticks % 4) == 0)
			g_Helpers.m_GameRunning = (vsh::GetCooperationMode() != vsh::CooperationMode::XMB);

		Sleep(250);
	}
}

void StartManualDnsWorker()
{

	sys_ppu_thread_t workerThreadId = SYS_PPU_THREAD_ID_INVALID;

	int createResult = sys_ppu_thread_create(&workerThreadId, [](uint64_t arg) -> void
	{
		ManualDnsWorker(arg);
		sys_ppu_thread_exit(0);
	}, 0, 1000, 0x4000, SYS_PPU_THREAD_CREATE_JOINABLE, "qDNSmDns");

}
