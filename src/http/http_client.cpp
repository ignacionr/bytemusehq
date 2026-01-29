#include "http_client.h"

// Include the appropriate implementation based on platform
#ifdef _WIN32
    #include "http_client_winhttp.h"
#else
    #include "http_client_curl.h"
#endif

#include <wx/log.h>
#include <mutex>

namespace Http {

std::unique_ptr<HttpClient> createHttpClient() {
    wxLogDebug("HTTP: Creating HTTP client for platform");
#ifdef _WIN32
    wxLogDebug("HTTP: Using WinHTTP backend");
    return std::make_unique<WinHttpClient>();
#else
    wxLogDebug("HTTP: Using CURL backend");
    return std::make_unique<CurlHttpClient>();
#endif
}

HttpClient& getHttpClient() {
    static std::once_flag flag;
    static std::unique_ptr<HttpClient> instance;
    
    std::call_once(flag, []() {
        instance = createHttpClient();
        if (instance->isAvailable()) {
            wxLogDebug("HTTP: Client initialized successfully (backend: %s)", 
                       instance->backendName());
        } else {
            wxLogError("HTTP: Client failed to initialize (backend: %s)", 
                       instance->backendName());
        }
    });
    
    return *instance;
}

} // namespace Http
