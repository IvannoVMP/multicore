/*
 * This file is part of the CitizenFX project - http://citizen.re/
 *
 * See LICENSE and MENTIONS in the root of the source tree for information
 * regarding licensing.
 */

#include "StdInc.h"
#include "HttpClient.h"
#include <VFSManager.h>
#include <sstream>

HttpClient::HttpClient(const wchar_t* userAgent)
{
	hWinHttp = WinHttpOpen(userAgent, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, WINHTTP_FLAG_ASYNC);
	WinHttpSetTimeouts(hWinHttp, 2000, 5000, 5000, 5000);
}

HttpClient::~HttpClient()
{
	WinHttpCloseHandle(hWinHttp);
}

void HttpClient::DoPostRequest(fwWString host, uint16_t port, fwWString url, fwMap<fwString, fwString>& fields, fwAction<bool, const char*, size_t> callback)
{
	fwString postData = BuildPostString(fields);

	DoPostRequest(host, port, url, postData, callback);
}

struct HttpClientRequestContext
{
	HttpClient* client;
	HttpClient::ServerPair server;
	HINTERNET hConnection;
	HINTERNET hRequest;
	fwString postData;
	fwAction<bool, const char*, size_t> callback;
	std::function<void(const std::map<std::string, std::string>&)> headerCallback;

	std::stringstream resultData;
	char buffer[32768];

	fwRefContainer<vfs::Device> outDevice;
	vfs::Device::THandle outHandle;

	std::string url;
	size_t getSize{ 0 };

	HttpClientRequestContext()
		: outDevice(nullptr)
	{

	}

	void DoCallback(bool success, fwString& resData)
	{
		if (outDevice.GetRef())
		{
			outDevice->Close(outHandle);
		}
		
		if (headerCallback)
		{
			std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> converter;
			std::map<std::string, std::string> headers;

			DWORD dwSize = 0;
			WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &dwSize, WINHTTP_NO_HEADER_INDEX);

			if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			{
				std::vector<wchar_t> buffer(dwSize);

				if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, &buffer[0], &dwSize, WINHTTP_NO_HEADER_INDEX))
				{
					wchar_t* lastHeaderName = nullptr;
					wchar_t* lastHeaderValue = nullptr;

					enum
					{
						STATE_NONE,
						STATE_NAME,
						STATE_VALUE
					} state = STATE_NONE;

					wchar_t* cursor = &buffer[0];

					while ((cursor - &buffer[0]) < buffer.size() && *cursor)
					{
						switch (state)
						{
							case STATE_NONE:
								state = STATE_NAME;
								lastHeaderName = cursor;
								break;

							case STATE_NAME:
								if (*cursor == L':')
								{
									*cursor = L'\0';
									++cursor;

									state = STATE_VALUE;
									lastHeaderValue = cursor + 1;
								}
								break;

							case STATE_VALUE:
								if (*cursor == L'\r')
								{
									*cursor = L'\0';
									++cursor;

									headers.insert({ converter.to_bytes(lastHeaderName), converter.to_bytes(lastHeaderValue) });

									state = STATE_NAME;
									lastHeaderName = cursor + 1;
								}
								break;
						}

						++cursor;
					}
				}
			}

			headerCallback(headers);
		}

		callback(success, resData.c_str(), (!resData.empty()) ? resData.size() : getSize);

		if (server.second)
		{
			client->ReaddConnection(server, hConnection);
		}
		else
		{
			WinHttpCloseHandle(hConnection);
		}

		WinHttpCloseHandle(hRequest);

		delete this;
	}
};

void HttpClient::DoPostRequest(fwWString host, uint16_t port, fwWString url, fwString postData, fwAction<bool, const char*, size_t> callback)
{
	DoPostRequest(host, port, url, postData, {}, callback);
}

void HttpClient::DoPostRequest(fwWString host, uint16_t port, fwWString url, fwString postData, const fwMap<fwString, fwString>& headers, fwAction<bool, const char*, size_t> callback, std::function<void(const std::map<std::string, std::string>&)> headerCallback)
{
	HINTERNET hConnection = WinHttpConnect(hWinHttp, host.c_str(), port, 0);
	HINTERNET hRequest = WinHttpOpenRequest(hConnection, L"POST", url.c_str(), 0, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);


	static std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> converter;

	for (auto& header : headers)
	{
		WinHttpAddRequestHeaders(hRequest, converter.from_bytes(va("%s: %s", header.first.c_str(), header.second.c_str())).c_str(), -1, 0);
	}

	WinHttpSetStatusCallback(hRequest, StatusCallback, WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS, 0);

	HttpClientRequestContext* context = new HttpClientRequestContext;
	context->client = this;
	context->hConnection = hConnection;
	context->hRequest = hRequest;
	context->postData = postData;
	context->callback = callback;
	context->headerCallback = headerCallback;

	context->url = "http://" + converter.to_bytes(host) + ":" + std::to_string(port) + converter.to_bytes(url);

	WinHttpSendRequest(hRequest, L"Content-Type: application/x-www-form-urlencoded; charset=utf-8\r\n", -1, const_cast<char*>(context->postData.c_str()), context->postData.length(), context->postData.length(), (DWORD_PTR)context);
}

void HttpClient::DoGetRequest(fwWString host, uint16_t port, fwWString url, fwAction<bool, const char*, size_t> callback)
{
	HINTERNET hConnection = WinHttpConnect(hWinHttp, host.c_str(), port, 0);
	HINTERNET hRequest = WinHttpOpenRequest(hConnection, L"GET", url.c_str(), 0, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);

	WinHttpSetStatusCallback(hRequest, StatusCallback, WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS, 0);

	HttpClientRequestContext* context = new HttpClientRequestContext;
	context->client = this;
	context->hConnection = hConnection;
	context->hRequest = hRequest;
	context->callback = callback;

	static std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> converter;
	context->url = "http://" + converter.to_bytes(host) + ":" + std::to_string(port) + converter.to_bytes(url);

	WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, nullptr, 0, 0, (DWORD_PTR)context);
}

void HttpClient::DoFileGetRequest(fwWString host, uint16_t port, fwWString url, const char* outDeviceBase, fwString outFilename, fwAction<bool, const char*, size_t> callback)
{
	DoFileGetRequest(host, port, url, vfs::GetDevice(outDeviceBase), outFilename, callback);
}

void HttpClient::ReaddConnection(ServerPair server, HINTERNET connection)
{
	m_connectionMutex.lock();

	if (!m_connectionFreeCBs.empty())
	{
		auto cb = m_connectionFreeCBs.front();
		m_connectionFreeCBs.pop();

		m_connectionMutex.unlock();

		cb(connection);

		return;
	}
	else
	{
		auto range = m_connections.equal_range(server);

		for (auto& it = range.first; it != range.second; it++)
		{
			if (!it->second)
			{
				it->second = connection;

				m_connectionMutex.unlock();

				return;
			}
		}
	}

	m_connectionMutex.unlock();
}

HINTERNET HttpClient::GetConnection(ServerPair server)
{
	auto range = m_connections.equal_range(server);

	int numConnections = 0;

	for (auto& it = range.first; it != range.second; it++)
	{
		numConnections++;

		if (it->second)
		{
			auto conn = it->second;
			it->second = nullptr;

			return conn;
		}
	}

	if (numConnections >= 8)
	{
		return nullptr;
	}

	HINTERNET hConnection = WinHttpConnect(hWinHttp, server.first.c_str(), server.second, 0);

	if (!hConnection)
	{
		return INVALID_HANDLE_VALUE;
	}

	m_connections.insert(std::make_pair(server, nullptr));

	return hConnection;
}

void HttpClient::QueueOnConnectionFree(fwAction<HINTERNET> cb)
{
	m_connectionFreeCBs.push(cb);

	// FIXME: possible race condition if a request just completed?
}

void HttpClient::DoFileGetRequest(fwWString host, uint16_t port, fwWString url, rage::fiDevice* outDevice, fwString outFilename, fwAction<bool, const char*, size_t> callback, HANDLE hConnection)
{
	return DoFileGetRequest(host, port, url, vfs::GetNativeDevice(outDevice), outFilename, callback, hConnection);
}

void HttpClient::DoFileGetRequest(fwWString host, uint16_t port, fwWString url, fwRefContainer<vfs::Device> outDevice, fwString outFilename, fwAction<bool, const char*, size_t> callback, HANDLE hConnection)
{
	ServerPair pair = std::make_pair(host, port);

	m_connectionMutex.lock();

	if (!hConnection)
	{
		hConnection = GetConnection(pair);

		if (hConnection == INVALID_HANDLE_VALUE)
		{
			callback(false, "", 0);

			m_connectionMutex.unlock();

			return;
		}

		if (!hConnection)
		{
			QueueOnConnectionFree([=] (HINTERNET connection)
			{
				DoFileGetRequest(host, port, url, outDevice, outFilename, callback, connection);
			});

			m_connectionMutex.unlock();

			return;
		}
	}

	m_connectionMutex.unlock();

	//HINTERNET hConnection = WinHttpConnect(hWinHttp, host.c_str(), port, 0);
	HINTERNET hRequest = WinHttpOpenRequest(hConnection, L"GET", url.c_str(), 0, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);

	WinHttpSetStatusCallback(hRequest, StatusCallback, WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS, 0);

	HttpClientRequestContext* context = new HttpClientRequestContext;
	context->client = this;
	context->hConnection = hConnection;
	context->hRequest = hRequest;
	context->callback = callback;
	context->outDevice = outDevice;
	context->server = pair;

	static std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> converter;
	context->url = "http://" + converter.to_bytes(host) + ":" + std::to_string(port) + converter.to_bytes(url);

	if (!context->outDevice.GetRef())
	{
		GlobalError("context->outDevice was null in " __FUNCTION__);
		return;
	}

	context->outHandle = context->outDevice->Create(outFilename.c_str());

	WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, nullptr, 0, 0, (DWORD_PTR)context);
}

void HttpClient::StatusCallback(HINTERNET handle, DWORD_PTR context, DWORD code, void* info, DWORD length)
{
	HttpClientRequestContext* ctx = (HttpClientRequestContext*)context;

	switch (code)
	{
		case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR:
		{
			WINHTTP_ASYNC_RESULT* infoStruct = reinterpret_cast<WINHTTP_ASYNC_RESULT*>(info);

			const char* apiCall = "unknown WinHTTP call";

			switch (infoStruct->dwResult)
			{
				case API_RECEIVE_RESPONSE:
					apiCall = "WinHttpReceiveResponse";
					break;
				case API_QUERY_DATA_AVAILABLE:
					apiCall = "WinHttpQueryDataAvailable";
					break;
				case API_READ_DATA:
					apiCall = "WinHttpReadData";
					break;
				case API_WRITE_DATA:
					apiCall = "WinHttpWriteData";
					break;
				case API_SEND_REQUEST:
					apiCall = "WinHttpSendRequest";
					break;
			}
			
			trace("%s on %s failed - error code %d\n", apiCall, ctx->url.c_str(), infoStruct->dwError);

			ctx->DoCallback(false, fwString());
			break;
		}

		case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
			if (!WinHttpReceiveResponse(ctx->hRequest, 0))
			{
				ctx->DoCallback(false, fwString());
			}

			break;

		case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:
		{
			uint32_t statusCode;
			DWORD statusCodeLength = sizeof(uint32_t);

			if (!WinHttpQueryHeaders(ctx->hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeLength, WINHTTP_NO_HEADER_INDEX))
			{
				ctx->DoCallback(false, fwString());
				return;
			}

			if (statusCode != HTTP_STATUS_OK)
			{
				ctx->DoCallback(false, fwString());
				return;
			}

			if (!WinHttpReadData(ctx->hRequest, ctx->buffer, sizeof(ctx->buffer) - 1, nullptr))
			{
				ctx->DoCallback(false, fwString());
			}

			break;
		}
		case WINHTTP_CALLBACK_STATUS_READ_COMPLETE:
			if (ctx->outDevice.GetRef())
			{
				ctx->outDevice->Write(ctx->outHandle, ctx->buffer, length);
			}
			else
			{
				ctx->buffer[length] = '\0';

				ctx->resultData << fwString(ctx->buffer, length);
			}

			ctx->getSize += length;

			if (length > 0)
			{
				if (!WinHttpReadData(ctx->hRequest, ctx->buffer, sizeof(ctx->buffer) - 1, nullptr))
				{
					ctx->DoCallback(false, fwString());
				}
			}
			else
			{
				std::string str = ctx->resultData.str();
				ctx->DoCallback(true, fwString(str.c_str(), str.size()));
			}

			break;
	}
}

bool HttpClient::CrackUrl(fwString url, fwWString& hostname, fwWString& path, uint16_t& port)
{
	wchar_t wideUrl[1024];
	mbstowcs(wideUrl, url.c_str(), _countof(wideUrl));
	wideUrl[1023] = L'\0';

	URL_COMPONENTS components = { 0 };
	components.dwStructSize = sizeof(components);

	components.dwHostNameLength = -1;
	components.dwUrlPathLength = -1;
	components.dwExtraInfoLength = -1;

	if (!WinHttpCrackUrl(wideUrl, wcslen(wideUrl), 0, &components))
	{
		return false;
	}

	hostname = fwWString(components.lpszHostName, components.dwHostNameLength);
	path = fwWString(components.lpszUrlPath, components.dwUrlPathLength);
	path += fwWString(components.lpszExtraInfo, components.dwExtraInfoLength);
	port = components.nPort;

	return true;
}

fwString HttpClient::BuildPostString(fwMap<fwString, fwString>& fields)
{
	std::stringstream retval;

	for (auto& field : fields)
	{
		retval << field.first << "=" << url_encode(field.second) << "&";
	}

	fwString str = fwString(retval.str().c_str());
	return str.substr(0, str.length() - 1);
}