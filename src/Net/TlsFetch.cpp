#include "TlsFetch.hpp"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <cell/cell_fs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/random_number.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/memory_buffer_alloc.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>

static const int kConnectTimeoutSeconds = 5;
static const int kIoTimeoutSeconds = 10;

static uint64_t s_MbedTlsArena[256 * 1024 / sizeof(uint64_t)];
static unsigned char s_ReceiveBuffer[64 * 1024];
static unsigned char s_BodyBuffer[64 * 1024];

static void Fail(NetProbeRequest& request, int stage, int error,
	int tlsError = 0, int netError = 0)
{
	request.stage = stage;
	request.error = error;
	request.tlsError = tlsError;
	request.netError = netError;
}

static bool Append(char*& cursor, char* end, const char* text)
{
	while (*text != '\0')
	{
		if (cursor >= end)
			return false;

		*cursor++ = *text++;
	}

	return true;
}

static bool ParseHttpsUrl(const char* url, char* host, size_t hostSize,
	char* path, size_t pathSize)
{
	static const char kPrefix[] = "https://";

	for (size_t i = 0; i < sizeof(kPrefix) - 1; ++i)
	{
		if (url[i] != kPrefix[i])
			return false;
	}

	const char* cursor = url + (sizeof(kPrefix) - 1);
	const char* hostEnd = cursor;

	while ((*hostEnd != '\0') && (*hostEnd != '/') && (*hostEnd != ':'))
		++hostEnd;

	const size_t hostLength = (size_t)(hostEnd - cursor);

	if ((hostLength == 0) || (hostLength >= hostSize))
		return false;

	memcpy(host, cursor, hostLength);
	host[hostLength] = '\0';

	if (*hostEnd == ':')
	{
		const char* port = hostEnd + 1;
		int value = 0;

		if ((*port < '0') || (*port > '9'))
			return false;

		while ((*port >= '0') && (*port <= '9'))
		{
			value = (value * 10) + (*port - '0');
			++port;
		}

		if (value != 443)
			return false;

		hostEnd = port;
	}

	if (*hostEnd == '/')
	{
		const size_t pathLength = strlen(hostEnd);

		if (pathLength >= pathSize)
			return false;

		memcpy(path, hostEnd, pathLength + 1);
	}
	else
	{
		if (pathSize < 2)
			return false;

		path[0] = '/';
		path[1] = '\0';
	}

	return true;
}

static int ConnectWithTimeout(int socketFd, const struct sockaddr_in& address,
	int* netError)
{
	int enabled = 1;

	if (setsockopt(socketFd, SOL_SOCKET, SO_NBIO, &enabled, sizeof(enabled)) != 0)
	{
		*netError = -1;
		return -1;
	}

	if (connect(socketFd, (const struct sockaddr*)&address, sizeof(address)) != 0)
	{
		fd_set writeSet;
		FD_ZERO(&writeSet);
		FD_SET(socketFd, &writeSet);

		struct timeval timeout;
		timeout.tv_sec = kConnectTimeoutSeconds;
		timeout.tv_usec = 0;

		const int selected = socketselect(
			socketFd + 1, nullptr, &writeSet, nullptr, &timeout);

		if (selected <= 0)
		{
			*netError = (selected == 0) ? -2 : -3;
			return -1;
		}

		int socketError = 0;
		socklen_t length = sizeof(socketError);

		if (getsockopt(socketFd, SOL_SOCKET, SO_ERROR,
			&socketError, &length) != 0)
		{
			*netError = -4;
			return -1;
		}

		if (socketError != 0)
		{
			*netError = socketError;
			return -1;
		}
	}

	enabled = 0;
	setsockopt(socketFd, SOL_SOCKET, SO_NBIO, &enabled, sizeof(enabled));

	struct timeval ioTimeout;
	ioTimeout.tv_sec = kIoTimeoutSeconds;
	ioTimeout.tv_usec = 0;

	setsockopt(socketFd, SOL_SOCKET, SO_RCVTIMEO, &ioTimeout, sizeof(ioTimeout));
	setsockopt(socketFd, SOL_SOCKET, SO_SNDTIMEO, &ioTimeout, sizeof(ioTimeout));

	return 0;
}

static int NetSend(void* context, const unsigned char* buffer, size_t length)
{
	const int socketFd = *static_cast<int*>(context);
	const int sent = (int)send(socketFd, buffer, length, 0);

	if (sent < 0)
		return MBEDTLS_ERR_NET_SEND_FAILED;

	if (sent == 0)
		return MBEDTLS_ERR_NET_CONN_RESET;

	return sent;
}

static int NetRecv(void* context, unsigned char* buffer, size_t length)
{
	const int socketFd = *static_cast<int*>(context);
	const int received = (int)recv(socketFd, buffer, length, 0);

	if (received < 0)
		return MBEDTLS_ERR_NET_RECV_FAILED;

	if (received == 0)
		return 0;

	return received;
}

static char ToLowerAscii(char character)
{
	if ((character >= 'A') && (character <= 'Z'))
		return (char)(character + ('a' - 'A'));

	return character;
}

static bool HeaderContains(const char* headers, size_t length, const char* name)
{
	const size_t nameLength = strlen(name);

	if (nameLength == 0)
		return true;

	for (size_t i = 0; (i + nameLength) <= length; ++i)
	{
		size_t j = 0;

		while ((j < nameLength) &&
			(ToLowerAscii(headers[i + j]) == ToLowerAscii(name[j])))
		{
			++j;
		}

		if (j == nameLength)
			return true;
	}

	return false;
}

static bool FindContentLength(const char* headers, size_t length,
	size_t& contentLength)
{
	static const char kName[] = "content-length:";
	const size_t nameLength = sizeof(kName) - 1;

	for (size_t i = 0; (i + nameLength) <= length; ++i)
	{
		size_t j = 0;

		while ((j < nameLength) &&
			(ToLowerAscii(headers[i + j]) == kName[j]))
		{
			++j;
		}

		if (j != nameLength)
			continue;

		size_t cursor = i + nameLength;

		while ((cursor < length) &&
			((headers[cursor] == ' ') || (headers[cursor] == '\t')))
		{
			++cursor;
		}

		size_t value = 0;
		bool any = false;

		while ((cursor < length) &&
			(headers[cursor] >= '0') && (headers[cursor] <= '9'))
		{
			value = (value * 10) + (size_t)(headers[cursor] - '0');
			any = true;
			++cursor;
		}

		if (!any)
			return false;

		contentLength = value;
		return true;
	}

	return false;
}

static bool DecodeChunked(const unsigned char* data, size_t length,
	unsigned char* output, size_t outputCapacity, size_t& outputLength)
{
	size_t cursor = 0;
	outputLength = 0;

	while (cursor < length)
	{
		size_t chunkSize = 0;
		bool any = false;

		while (cursor < length)
		{
			const char character = (char)data[cursor];
			int value = 0;

			if ((character >= '0') && (character <= '9'))
				value = character - '0';
			else if ((character >= 'a') && (character <= 'f'))
				value = (character - 'a') + 10;
			else if ((character >= 'A') && (character <= 'F'))
				value = (character - 'A') + 10;
			else
				break;

			chunkSize = (chunkSize * 16) + (size_t)value;
			any = true;
			++cursor;
		}

		if (!any)
			return false;

		while (((cursor + 1) < length) &&
			!((data[cursor] == '\r') && (data[cursor + 1] == '\n')))
		{
			++cursor;
		}

		if ((cursor + 1) >= length)
			return false;

		cursor += 2;

		if (chunkSize == 0)
			return true;

		if (chunkSize > (length - cursor))
			return false;

		if ((outputLength + chunkSize) > outputCapacity)
			return false;

		memcpy(output + outputLength, data + cursor, chunkSize);
		outputLength += chunkSize;
		cursor += chunkSize;

		if (((cursor + 1) >= length) ||
			(data[cursor] != '\r') || (data[cursor + 1] != '\n'))
		{
			return false;
		}

		cursor += 2;
	}

	return false;
}

static const unsigned char* FindHeaderEnd(const unsigned char* data,
	size_t length, size_t& headerLength)
{
	for (size_t i = 0; (i + 3) < length; ++i)
	{
		if ((data[i] == '\r') && (data[i + 1] == '\n') &&
			(data[i + 2] == '\r') && (data[i + 3] == '\n'))
		{
			headerLength = i + 4;
			return data + i + 4;
		}
	}

	return nullptr;
}

static int ParseStatusCode(const unsigned char* data, size_t length)
{
	size_t cursor = 0;

	while (cursor < length)
	{
		if ((data[cursor] == ' ') &&
			((cursor + 3) < length) &&
			(data[cursor + 1] >= '0') && (data[cursor + 1] <= '9') &&
			(data[cursor + 2] >= '0') && (data[cursor + 2] <= '9') &&
			(data[cursor + 3] >= '0') && (data[cursor + 3] <= '9'))
		{
			return ((data[cursor + 1] - '0') * 100) +
				((data[cursor + 2] - '0') * 10) +
				(data[cursor + 3] - '0');
		}

		++cursor;
	}

	return -1;
}

static bool WriteFile(const char* path, const unsigned char* data, size_t length)
{
	int fileHandle = -1;
	const int flags = CELL_FS_O_CREAT | CELL_FS_O_TRUNC | CELL_FS_O_WRONLY;

	if (cellFsOpen(path, flags, &fileHandle, nullptr, 0) != CELL_FS_SUCCEEDED)
		return false;

	uint64_t written = 0;
	const int result = cellFsWrite(fileHandle, data, (uint64_t)length, &written);

	cellFsClose(fileHandle);

	return (result == CELL_FS_SUCCEEDED) && (written == (uint64_t)length);
}

extern "C" void* qDNSmMbedtlsStubCalloc(size_t count, size_t size)
{
	(void)count;
	(void)size;
	return nullptr;
}

extern "C" void qDNSmMbedtlsStubFree(void* pointer)
{
	(void)pointer;
}

extern "C" void qDNSmMbedtlsExit(int status)
{
	(void)status;

	/* Only reachable on buffer-allocator corruption; never return. */
	for (;;)
	{
	}
}

extern "C" int mbedtls_hardware_poll(void* data, unsigned char* output,
	size_t length, size_t* outputLength)
{
	(void)data;

	if ((output == nullptr) || (outputLength == nullptr))
		return -1;

	for (size_t done = 0; done < length; )
	{
		uint64_t chunk = (uint64_t)(length - done);

		if (chunk > 4096)
			chunk = 4096;

		const int result = sys_get_random_number(output + done, chunk);

		if (result != 0)
			return result;

		done += (size_t)chunk;
	}

	*outputLength = length;
	return 0;
}

void RunTlsProbe(NetProbeRequest& request)
{
	Fail(request, kNetProbeStageStart, kNetProbeErrorNone);
	request.httpStatus = 0;
	request.bytes = 0;

	if (request.url[0] == '\0')
	{
		Fail(request, kNetProbeStageStart, kNetProbeErrorArgs);
		return;
	}

	if (request.outputPath[0] == '\0')
	{
		Fail(request, kNetProbeStageStart, kNetProbeErrorArgs);
		return;
	}

	char host[256];
	char path[256];

	if (!ParseHttpsUrl(request.url, host, sizeof(host), path, sizeof(path)))
	{
		Fail(request, kNetProbeStageStart, kNetProbeErrorArgs);
		return;
	}

	request.stage = kNetProbeStageDns;

	struct hostent* hostEntry = gethostbyname(host);

	if ((hostEntry == nullptr) ||
		(hostEntry->h_addrtype != AF_INET) ||
		(hostEntry->h_length != 4) ||
		(hostEntry->h_addr_list == nullptr) ||
		(hostEntry->h_addr_list[0] == nullptr))
	{
		Fail(request, kNetProbeStageDns, kNetProbeErrorDns);
		return;
	}

	request.stage = kNetProbeStageSocket;

	const int socketFd = (int)socket(AF_INET, SOCK_STREAM, 0);

	if (socketFd < 0)
	{
		Fail(request, kNetProbeStageSocket, kNetProbeErrorSocket, 0, socketFd);
		return;
	}

	struct sockaddr_in address;
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_port = htons(443);
	memcpy(&address.sin_addr, hostEntry->h_addr_list[0], 4);

	request.stage = kNetProbeStageConnect;

	int netError = 0;

	if (ConnectWithTimeout(socketFd, address, &netError) != 0)
	{
		socketclose(socketFd);
		Fail(request, kNetProbeStageConnect, kNetProbeErrorConnect, 0, netError);
		return;
	}

	mbedtls_memory_buffer_alloc_init(
		reinterpret_cast<unsigned char*>(s_MbedTlsArena), sizeof(s_MbedTlsArena));

	mbedtls_entropy_context entropy;
	mbedtls_ctr_drbg_context drbg;
	mbedtls_ssl_context ssl;
	mbedtls_ssl_config config;

	mbedtls_entropy_init(&entropy);
	mbedtls_ctr_drbg_init(&drbg);
	mbedtls_ssl_init(&ssl);
	mbedtls_ssl_config_init(&config);

	request.stage = kNetProbeStageTlsConfig;

	static const unsigned char kPersonalization[] = "qDNSmNetLib";

	int result = mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy,
		kPersonalization, sizeof(kPersonalization) - 1);

	if (result == 0)
	{
		result = mbedtls_ssl_config_defaults(&config, MBEDTLS_SSL_IS_CLIENT,
			MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
	}

	if (result == 0)
	{
		mbedtls_ssl_conf_authmode(&config, MBEDTLS_SSL_VERIFY_NONE);
		mbedtls_ssl_conf_rng(&config, mbedtls_ctr_drbg_random, &drbg);
		mbedtls_ssl_conf_min_version(
			&config, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);
		mbedtls_ssl_conf_max_version(
			&config, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);
		result = mbedtls_ssl_setup(&ssl, &config);
	}

	if (result == 0)
		result = mbedtls_ssl_set_hostname(&ssl, host);

	if (result != 0)
	{
		Fail(request, kNetProbeStageTlsConfig, kNetProbeErrorTlsConfig, result);
		goto cleanup;
	}

	mbedtls_ssl_set_bio(&ssl, (void*)&socketFd, NetSend, NetRecv, nullptr);

	request.stage = kNetProbeStageTlsHandshake;

	for (;;)
	{
		result = mbedtls_ssl_handshake(&ssl);

		if (result == 0)
			break;

		if ((result == MBEDTLS_ERR_SSL_WANT_READ) ||
			(result == MBEDTLS_ERR_SSL_WANT_WRITE))
		{
			continue;
		}

		Fail(request, kNetProbeStageTlsHandshake,
			kNetProbeErrorTlsHandshake, result);
		goto cleanup;
	}

	request.stage = kNetProbeStageHttp;

	char httpRequest[512];
	char* cursor = httpRequest;
	char* const end = httpRequest + sizeof(httpRequest) - 1;

	const bool requestBuilt =
		Append(cursor, end, "GET ") &&
		Append(cursor, end, path) &&
		Append(cursor, end, " HTTP/1.1\r\nHost: ") &&
		Append(cursor, end, host) &&
		Append(cursor, end,
			"\r\nUser-Agent: qDNSm/1.1\r\nAccept: */*\r\n"
			"Accept-Encoding: identity\r\nConnection: close\r\n\r\n");

	if (!requestBuilt)
	{
		Fail(request, kNetProbeStageHttp, kNetProbeErrorArgs);
		goto cleanup;
	}

	const size_t httpRequestLength = (size_t)(cursor - httpRequest);
	size_t sent = 0;

	while (sent < httpRequestLength)
	{
		result = mbedtls_ssl_write(&ssl,
			reinterpret_cast<const unsigned char*>(httpRequest) + sent,
			httpRequestLength - sent);

		if ((result == MBEDTLS_ERR_SSL_WANT_READ) ||
			(result == MBEDTLS_ERR_SSL_WANT_WRITE))
		{
			continue;
		}

		if (result <= 0)
		{
			Fail(request, kNetProbeStageHttp, kNetProbeErrorHttp, result);
			goto cleanup;
		}

		sent += (size_t)result;
	}

	size_t received = 0;

	while (received < (sizeof(s_ReceiveBuffer) - 1))
	{
		result = mbedtls_ssl_read(&ssl,
			s_ReceiveBuffer + received,
			(sizeof(s_ReceiveBuffer) - 1) - received);

		if (result > 0)
		{
			received += (size_t)result;
			continue;
		}

		if (result == 0)
			break;

		if ((result == MBEDTLS_ERR_SSL_WANT_READ) ||
			(result == MBEDTLS_ERR_SSL_WANT_WRITE))
		{
			continue;
		}

		if (result == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)
			break;

		if (received > 0)
			break;

		Fail(request, kNetProbeStageHttp, kNetProbeErrorHttp, result);
		goto cleanup;
	}

	if (received == 0)
	{
		Fail(request, kNetProbeStageHttp, kNetProbeErrorHttp);
		goto cleanup;
	}

	s_ReceiveBuffer[received] = '\0';

	size_t headerLength = 0;
	const unsigned char* body = FindHeaderEnd(s_ReceiveBuffer, received, headerLength);

	if (body == nullptr)
	{
		Fail(request, kNetProbeStageHttp, kNetProbeErrorHttp);
		goto cleanup;
	}

	const int status = ParseStatusCode(s_ReceiveBuffer, received);

	request.httpStatus = status;

	if ((status < 200) || (status > 299))
	{
		Fail(request, kNetProbeStageHttp, kNetProbeErrorHttp);
		goto cleanup;
	}

	const size_t bodyLength = received - headerLength;
	const char* headers = reinterpret_cast<const char*>(s_ReceiveBuffer);
	size_t writeLength = bodyLength;
	const unsigned char* writeData = body;

	if (HeaderContains(headers, headerLength, "transfer-encoding:") &&
		HeaderContains(headers, headerLength, "chunked"))
	{
		size_t decodedLength = 0;

		if (!DecodeChunked(body, bodyLength, s_BodyBuffer,
			sizeof(s_BodyBuffer), decodedLength))
		{
			Fail(request, kNetProbeStageHttp, kNetProbeErrorHttp);
			goto cleanup;
		}

		writeData = s_BodyBuffer;
		writeLength = decodedLength;
	}
	else
	{
		size_t contentLength = 0;

		if (FindContentLength(headers, headerLength, contentLength))
		{
			if (contentLength > bodyLength)
			{
				Fail(request, kNetProbeStageHttp, kNetProbeErrorHttp);
				goto cleanup;
			}

			writeLength = contentLength;
		}
	}

	request.stage = kNetProbeStageFile;

	if (!WriteFile(request.outputPath, writeData, writeLength))
	{
		Fail(request, kNetProbeStageFile, kNetProbeErrorFile);
		goto cleanup;
	}

	request.bytes = (uint32_t)writeLength;
	request.stage = kNetProbeStageDone;
	request.error = kNetProbeErrorNone;

cleanup:
	mbedtls_ssl_free(&ssl);
	mbedtls_ssl_config_free(&config);
	mbedtls_ctr_drbg_free(&drbg);
	mbedtls_entropy_free(&entropy);
	mbedtls_memory_buffer_alloc_free();

	socketclose(socketFd);
}
