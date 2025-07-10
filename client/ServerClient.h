// This file is a part of RCKangaroo software
// (c) 2024, RetiredCoder (RC)
// License: GPLv3, see "LICENSE.TXT" file
// https://github.com/RetiredC

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <curl/curl.h>
#include "../defs.h"

// Structure for work assignment from server
struct ServerWork {
    std::string range_id;
    std::string start_range;
    std::string end_range;
    int bit_range;
    int dp_bits;
    std::string pubkey;
};

// Structure for Distinguished Point to send to server
struct ServerDP {
    std::string x_coord;    // 12 bytes hex
    std::string distance;   // 22 bytes hex
    int kang_type;          // 0=TAME, 1=WILD1, 2=WILD2
};

// HTTP response structure
struct HttpResponse {
    std::string data;
    long response_code;
    bool success;
};

class ServerClient {
private:
    std::string server_url;
    std::string client_id;
    CURL* curl;
    
    // HTTP callback for writing response data
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* data);
    
    // Make HTTP POST request
    HttpResponse makePost(const std::string& endpoint, const std::string& json_data);
    
    // Make HTTP GET request
    HttpResponse makeGet(const std::string& endpoint);
    
    // Convert binary data to hex string
    std::string bytesToHex(const u8* data, size_t length);
    
public:
    ServerClient(const std::string& server_url, const std::string& client_id);
    ~ServerClient();
    
    // Initialize curl
    bool initialize();
    
    // Cleanup curl
    void cleanup();
    
    // Configure server search parameters
    bool configureSearch(const std::string& start_range, const std::string& end_range, 
                        const std::string& pubkey, int dp_bits, const std::string& range_size);
    
    // Get work assignment from server
    std::unique_ptr<ServerWork> getWork();
    
    // Submit distinguished points to server
    bool submitPoints(const std::vector<ServerDP>& points, std::string& response_status, std::string& solution);
    
    // Get server status
    bool getStatus(std::string& status_json);
    
    // Check if server has found solution
    bool checkSolved(std::string& solution);
    
    // Convert DP record to ServerDP format
    ServerDP convertDP(const u8* dp_data);
};

// Global server client instance
extern ServerClient* g_serverClient;
extern bool g_useServer;
extern std::string g_currentRangeId;