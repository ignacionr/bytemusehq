#include "http_client.h"

// Include the appropriate implementation based on platform
#ifdef _WIN32
    #include "http_client_winhttp.h"
#else
    #include "http_client_curl.h"
#endif

#include <mutex>

namespace Http {

std::unique_ptr<HttpClient> createHttpClient() {
#ifdef _WIN32
    return std::make_unique<WinHttpClient>();
#else
    return std::make_unique<CurlHttpClient>();
#endif
}

HttpClient& getHttpClient() {
    static std::once_flag flag;
    static std::unique_ptr<HttpClient> instance;
    
    std::call_once(flag, []() {
        instance = createHttpClient();
    });
    
    return *instance;
}

} // namespace Http
