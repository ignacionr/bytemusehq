#include "../src/lsp/lsp_client.h"
#include <iostream>
#include <thread>
#include <chrono>

int main(int argc, char** argv) {
    std::string workspace = "/Users/inz/src/bytemusehq";
    std::string targetFile = "file:///Users/inz/src/bytemusehq/src/ai/ai_provider_gemini.h";
    
    if (argc > 1) {
        targetFile = argv[1];
    }
    
    std::cout << "Connecting to clangd for workspace: " << workspace << std::endl;
    std::cout << "Target file: " << targetFile << std::endl;
    
    LspClient client;
    
    client.setLogCallback([](const std::string& msg) {
        std::cout << "[LSP] " << msg << std::endl;
    });
    
    if (!client.start("clangd", workspace)) {
        std::cerr << "Failed to start clangd" << std::endl;
        return 1;
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    bool initialized = false;
    client.initialize([&initialized](bool success) {
        std::cout << "Initialization: " << (success ? "SUCCESS" : "FAILED") << std::endl;
        initialized = success;
    });
    
    // Wait for initialization
    for (int i = 0; i < 50 && !initialized; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    if (!initialized) {
        std::cerr << "Initialization timeout" << std::endl;
        return 1;
    }
    
    std::cout << "\n=== Querying server statistics ===" << std::endl;
    
    // Request $/memoryUsage (clangd extension)
    glz::generic memParams;
    client.sendCustomRequest("$/memoryUsage", memParams, [](const glz::generic& result) {
        std::cout << "Memory usage response:" << std::endl;
        std::cout << glz::write_json(result).value_or("{}") << std::endl;
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    // Open the target file to trigger indexing
    std::cout << "\n=== Opening target file to trigger indexing ===" << std::endl;
    
    // Read the actual file content
    std::ifstream file(targetFile.substr(7)); // Remove "file://" prefix
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    
    client.didOpen(targetFile, "cpp", content);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    
    // Try to get symbols from the file
    std::cout << "\n=== Requesting document symbols ===" << std::endl;
    bool gotSymbols = false;
    client.getDocumentSymbols(targetFile, [&gotSymbols](const std::vector<LspDocumentSymbol>& symbols) {
        std::cout << "Found " << symbols.size() << " symbols" << std::endl;
        for (size_t i = 0; i < std::min(size_t(5), symbols.size()); i++) {
            std::cout << "  - " << symbols[i].name << " (" << static_cast<int>(symbols[i].kind) << ")" << std::endl;
        }
        gotSymbols = true;
    });
    
    // Wait for response
    for (int i = 0; i < 50 && !gotSymbols; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "\n=== Diagnostic complete ===" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    client.stop();
    return 0;
}
