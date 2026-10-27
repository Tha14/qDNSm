#pragma once

#include "NetProbeProtocol.hpp"

/*
 * Performs one qDNSmNetLib fetch. Resolves the request URL, connects over TCP,
 * performs a TLS 1.2 handshake with mbedTLS, performs one HTTPS GET, decodes a
 * chunked or content-length body, writes it to request.outputPath, and fills in
 * the stage/error/status/bytes fields. Runs synchronously on the caller's
 * thread; module_start returns when the fetch has finished or failed.
 */
void RunTlsProbe(NetProbeRequest& request);
