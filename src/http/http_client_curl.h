#ifndef HTTP_CLIENT_CURL_H
#define HTTP_CLIENT_CURL_H

#include "http_client.h"
#include <wx/log.h>
#include <curl/curl.h>

namespace Http {

/**
 * HTTP client implementation using libcurl.
 * Used on macOS, Linux, and optionally on Windows.
 */
class CurlHttpClient : public HttpClient {
public:
    CurlHttpClient() {
        // Initialize CURL globally (safe to call multiple times)
        CURLcode res = curl_global_init(CURL_GLOBAL_DEFAULT);
        if (res != CURLE_OK) {
            wxLogError("HTTP: Failed to initialize CURL: %s", curl_easy_strerror(res));
        } else {
            wxLogDebug("HTTP: CURL client initialized (version: %s)", curl_version());
        }
    }
    
    ~CurlHttpClient() override {
        // Note: We don't call curl_global_cleanup() here because
        // other parts of the application might still use CURL.
        // It should be called once at application shutdown.
    }
    
    HttpResponse perform(const HttpRequest& request) override {
        HttpResponse response;
        
        wxLogDebug("HTTP: CURL perform() - %s %s", request.method, request.url);
        
        CURL* curl = curl_easy_init();
        if (!curl) {
            response.error = "Failed to initialize CURL";
            wxLogError("HTTP: %s", response.error);
            return response;
        }
        
        // Set up headers
        struct curl_slist* headerList = nullptr;
        for (const auto& [key, value] : request.headers) {
            std::string header = key + ": " + value;
            headerList = curl_slist_append(headerList, header.c_str());
        }
        
        // Response body collection
        std::string responseBody;
        
        // Configure CURL options
        curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, request.timeoutSeconds);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, request.verifySsl ? 1L : 0L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, request.followRedirects ? 1L : 0L);
        
        // Set method and body
        if (request.method == "POST") {
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(request.body.size()));
        } else if (request.method == "PUT") {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(request.body.size()));
        } else if (request.method == "DELETE") {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        } else if (request.method == "PATCH") {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(request.body.size()));
        }
        // GET is the default
        
        wxLogDebug("HTTP: Sending request (body size: %zu bytes)", request.body.size());
        
        // Perform the request
        CURLcode res = curl_easy_perform(curl);
        
        // Get HTTP status code
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.statusCode);
        
        // Cleanup
        curl_slist_free_all(headerList);
        curl_easy_cleanup(curl);
        
        if (res != CURLE_OK) {
            response.error = std::string("CURL error: ") + curl_easy_strerror(res);
            wxLogError("HTTP: %s (code=%d)", response.error, static_cast<int>(res));
            return response;
        }
        
        response.body = std::move(responseBody);
        response.success = (response.statusCode >= 200 && response.statusCode < 300);
        
        wxLogDebug("HTTP: Request complete - status=%ld, success=%s, body=%zu bytes",
                   response.statusCode, response.success ? "true" : "false",
                   response.body.size());
        
        return response;
    }
    
    bool isAvailable() const override {
        return true;
    }
    
    std::string backendName() const override {
        return "CURL";
    }
    
private:
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
        size_t totalSize = size * nmemb;
        userp->append(static_cast<char*>(contents), totalSize);
        return totalSize;
    }
};

} // namespace Http

#endif // HTTP_CLIENT_CURL_H
