#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <string>
#include <map>
#include <memory>

namespace Http {

/**
 * HTTP response structure containing the result of an HTTP request.
 */
struct HttpResponse {
    std::string body;           // Response body
    long statusCode = 0;        // HTTP status code (200, 404, etc.)
    std::string error;          // Error message if request failed
    bool success = false;       // True if request completed with 2xx status
    
    bool isOk() const { return success && statusCode >= 200 && statusCode < 300; }
    bool isClientError() const { return statusCode >= 400 && statusCode < 500; }
    bool isServerError() const { return statusCode >= 500; }
};

/**
 * HTTP request configuration.
 */
struct HttpRequest {
    std::string url;
    std::string method = "GET";
    std::string body;
    std::map<std::string, std::string> headers;
    long timeoutSeconds = 30;
    bool followRedirects = true;
    bool verifySsl = true;
};

/**
 * Abstract base class for HTTP client implementations.
 * Provides a platform-agnostic interface for making HTTP requests.
 */
class HttpClient {
public:
    virtual ~HttpClient() = default;
    
    /**
     * Perform an HTTP request and return the response.
     */
    virtual HttpResponse perform(const HttpRequest& request) = 0;
    
    /**
     * Check if the client is available/properly initialized.
     */
    virtual bool isAvailable() const = 0;
    
    /**
     * Get the name of the HTTP backend (e.g., "CURL", "WinHTTP").
     */
    virtual std::string backendName() const = 0;
    
    // Convenience methods
    HttpResponse get(const std::string& url, 
                     const std::map<std::string, std::string>& headers = {}) {
        HttpRequest req;
        req.url = url;
        req.method = "GET";
        req.headers = headers;
        return perform(req);
    }
    
    HttpResponse post(const std::string& url, 
                      const std::string& body,
                      const std::map<std::string, std::string>& headers = {}) {
        HttpRequest req;
        req.url = url;
        req.method = "POST";
        req.body = body;
        req.headers = headers;
        return perform(req);
    }
    
    HttpResponse put(const std::string& url, 
                     const std::string& body,
                     const std::map<std::string, std::string>& headers = {}) {
        HttpRequest req;
        req.url = url;
        req.method = "PUT";
        req.body = body;
        req.headers = headers;
        return perform(req);
    }
    
    HttpResponse del(const std::string& url,
                     const std::map<std::string, std::string>& headers = {}) {
        HttpRequest req;
        req.url = url;
        req.method = "DELETE";
        req.headers = headers;
        return perform(req);
    }
};

/**
 * Factory function to create the platform-appropriate HTTP client.
 * Defined in http_client.cpp
 */
std::unique_ptr<HttpClient> createHttpClient();

/**
 * Get a shared instance of the HTTP client (singleton pattern).
 * Thread-safe, lazily initialized.
 */
HttpClient& getHttpClient();

} // namespace Http

#endif // HTTP_CLIENT_H
