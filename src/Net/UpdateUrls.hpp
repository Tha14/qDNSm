#pragma once

/*
 * Update sources for the network module.
 *
 * Both files live in the repository's data/ directory on master and are
 * fetched over TLS 1.2.
 */

static const char* const kDnsListUrl =
	"https://raw.githubusercontent.com/Tha14/qDNSm/master/data/dns-servers.ini";

static const char* const kGameListUrl =
	"https://raw.githubusercontent.com/Tha14/qDNSm/master/data/qDNSm-server-games.ini";
