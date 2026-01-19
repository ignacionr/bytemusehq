#ifndef HTTP_CLIENT_WINHTTP_H
#define HTTP_CLIENT_WINHTTP_H

#include "http_client.h"

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <sstream>

#pragma comment(lib, "winhttp.lib")

namespace Http {

/**
 * HTTP client implementation using WinHTTP.
 * Windows-native HTTP client, available on all Windows versions.
 */
class WinHttpClient : public HttpClient {
public:
    WinHttpClient() {
        // Create a session handle
        m_session = WinHttpOpen(
            L"ByteMuseHQ/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0
        );
    }
    
    ~WinHttpClient() override {
        if (m_session) {
            WinHttpCloseHandle(m_session);
        }
    }
    
    HttpResponse perform(const HttpRequest& request) override {
        HttpResponse response;
        
        if (!m_session) {
            response.error = "WinHTTP session not initialized";
            return response;
        }
        
        // Parse URL
        URL_COMPONENTS urlComp = {};
        urlComp.dwStructSize = sizeof(urlComp);
        urlComp.dwSchemeLength = (DWORD)-1;
        urlComp.dwHostNameLength = (DWORD)-1;
        urlComp.dwUrlPathLength = (DWORD)-1;
        urlComp.dwExtraInfoLength = (DWORD)-1;
        
        std::wstring wUrl = toWideString(request.url);
        
        if (!WinHttpCrackUrl(wUrl.c_str(), static_cast<DWORD>(wUrl.length()), 0, &urlComp)) {
            response.error = "Failed to parse URL";
            return response;
        }
        
        // Extract hostname
        std::wstring hostName(urlComp.lpszHostName, urlComp.dwHostNameLength);
        std::wstring urlPath(urlComp.lpszUrlPath, urlComp.dwUrlPathLength);
        if (urlComp.dwExtraInfoLength > 0) {
            urlPath += std::wstring(urlComp.lpszExtraInfo, urlComp.dwExtraInfoLength);
        }
        
        // Determine port
        INTERNET_PORT port = urlComp.nPort;
        if (port == 0) {
            port = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? INTERNET_DEFAULT_HTTPS_PORT 
                                                               : INTERNET_DEFAULT_HTTP_PORT;
        }
        
        // Create connection handle
        HINTERNET hConnect = WinHttpConnect(m_session, hostName.c_str(), port, 0);
        if (!hConnect) {
            response.error = "Failed to connect: " + getLastErrorMessage();
            return response;
        }
        
        // Set up request flags
        DWORD requestFlags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
        
        // Create request handle
        std::wstring wMethod = toWideString(request.method);
        HINTERNET hRequest = WinHttpOpenRequest(
            hConnect,
            wMethod.c_str(),
            urlPath.c_str(),
            nullptr,  // HTTP/1.1
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            requestFlags
        );
        
        if (!hRequest) {
            response.error = "Failed to open request: " + getLastErrorMessage();
            WinHttpCloseHandle(hConnect);
            return response;
        }
        
        // Set timeout
        DWORD timeout = static_cast<DWORD>(request.timeoutSeconds * 1000);
        WinHttpSetTimeouts(hRequest, timeout, timeout, timeout, timeout);
        
        // Set SSL verification options
        if (!request.verifySsl && urlComp.nScheme == INTERNET_SCHEME_HTTPS) {
            DWORD securityFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                                  SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                                  SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
            WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, 
                           &securityFlags, sizeof(securityFlags));
        }
        
        // Set redirect option
        if (!request.followRedirects) {
            DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
            WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY,
                           &redirectPolicy, sizeof(redirectPolicy));
        }
        
        // Build headers string
        std::wstring headers;
        for (const auto& [key, value] : request.headers) {
            headers += toWideString(key) + L": " + toWideString(value) + L"\r\n";
        }
        
        // Add headers to request
        if (!headers.empty()) {
            WinHttpAddRequestHeaders(hRequest, headers.c_str(), 
                                    static_cast<DWORD>(headers.length()),
                                    WINHTTP_ADDREQ_FLAG_ADD);
        }
        
        // Send request
        BOOL sendResult;
        if (!request.body.empty()) {
            sendResult = WinHttpSendRequest(
                hRequest,
                WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                const_cast<char*>(request.body.c_str()),
                static_cast<DWORD>(request.body.size()),
                static_cast<DWORD>(request.body.size()),
                0
            );
        } else {
            sendResult = WinHttpSendRequest(
                hRequest,
                WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                WINHTTP_NO_REQUEST_DATA, 0,
                0, 0
            );
        }
        
        if (!sendResult) {
            response.error = "Failed to send request: " + getLastErrorMessage();
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            return response;
        }
        
        // Receive response
        if (!WinHttpReceiveResponse(hRequest, nullptr)) {
            response.error = "Failed to receive response: " + getLastErrorMessage();
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            return response;
        }
        
        // Get status code
        DWORD statusCode = 0;
        DWORD statusCodeSize = sizeof(statusCode);
        WinHttpQueryHeaders(hRequest,
                           WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                           WINHTTP_HEADER_NAME_BY_INDEX,
                           &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX);
        response.statusCode = static_cast<long>(statusCode);
        
        // Read response body
        std::string responseBody;
        DWORD bytesAvailable = 0;
        
        do {
            bytesAvailable = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &bytesAvailable)) {
                break;
            }
            
            if (bytesAvailable == 0) {
                break;
            }
            
            std::vector<char> buffer(bytesAvailable + 1);
            DWORD bytesRead = 0;
            
            if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead)) {
                responseBody.append(buffer.data(), bytesRead);
            }
        } while (bytesAvailable > 0);
        
        // Cleanup
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        
        response.body = std::move(responseBody);
        response.success = (response.statusCode >= 200 && response.statusCode < 300);
        return response;
    }
    
    bool isAvailable() const override {
        return m_session != nullptr;
    }
    
    std::string backendName() const override {
        return "WinHTTP";
    }
    
private:
    HINTERNET m_session = nullptr;
    
    static std::wstring toWideString(const std::string& str) {
        if (str.empty()) return L"";
        
        int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), 
                                             static_cast<int>(str.size()), nullptr, 0);
        std::wstring result(sizeNeeded, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), 
                           static_cast<int>(str.size()), &result[0], sizeNeeded);
        return result;
    }
    
    static std::string getLastErrorMessage() {
        DWORD error = GetLastError();
        if (error == 0) return "";
        
        LPSTR messageBuffer = nullptr;
        DWORD size = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_FROM_HMODULE,
            GetModuleHandleA("winhttp.dll"),
            error,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPSTR>(&messageBuffer),
            0, nullptr
        );
        
        if (size == 0) {
            // Try without winhttp module
            size = FormatMessageA(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                nullptr,
                error,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                reinterpret_cast<LPSTR>(&messageBuffer),
                0, nullptr
            );
        }
        
        std::string message;
        if (size > 0 && messageBuffer) {
            message = std::string(messageBuffer, size);
            // Trim trailing whitespace
            while (!message.empty() && (message.back() == '\n' || message.back() == '\r')) {
                message.pop_back();
            }
            LocalFree(messageBuffer);
        } else {
            message = "Unknown error " + std::to_string(error);
        }
        
        return message;
    }
};

} // namespace Http

#endif // _WIN32

#endif // HTTP_CLIENT_WINHTTP_H
