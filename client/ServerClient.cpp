// This file is a part of RCKangaroo software
// (c) 2024, RetiredCoder (RC)
// License: GPLv3, see "LICENSE.TXT" file
// https://github.com/RetiredC

#include "ServerClient.h"
#include <json/json.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>

// Global instances
ServerClient* g_serverClient = nullptr;
bool g_useServer = false;
std::string g_currentRangeId = "";

ServerClient::ServerClient(const std::string& server_url, const std::string& client_id)
    : server_url(server_url), client_id(client_id), curl(nullptr) {
}

ServerClient::~ServerClient() {
    cleanup();
}

bool ServerClient::initialize() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize curl" << std::endl;
        return false;
    }
    return true;
}

void ServerClient::cleanup() {
    if (curl) {
        curl_easy_cleanup(curl);
        curl = nullptr;
    }
    curl_global_cleanup();
}

size_t ServerClient::WriteCallback(void* contents, size_t size, size_t nmemb, std::string* data) {
    size_t total_size = size * nmemb;
    data->append((char*)contents, total_size);
    return total_size;
}

HttpResponse ServerClient::makePost(const std::string& endpoint, const std::string& json_data) {
    HttpResponse response;
    response.success = false;
    response.response_code = 0;
    
    if (!curl) {
        return response;
    }
    
    std::string url = server_url + endpoint;
    std::string response_data;
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    CURLcode res = curl_easy_perform(curl);
    
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.response_code);
        response.data = response_data;
        response.success = (response.response_code >= 200 && response.response_code < 300);
    }
    
    curl_slist_free_all(headers);
    return response;
}

HttpResponse ServerClient::makeGet(const std::string& endpoint) {
    HttpResponse response;
    response.success = false;
    response.response_code = 0;
    
    if (!curl) {
        return response;
    }
    
    std::string url = server_url + endpoint;
    std::string response_data;
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    
    CURLcode res = curl_easy_perform(curl);
    
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.response_code);
        response.data = response_data;
        response.success = (response.response_code >= 200 && response.response_code < 300);
    }
    
    return response;
}

std::string ServerClient::bytesToHex(const u8* data, size_t length) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < length; i++) {
        ss << std::setw(2) << static_cast<unsigned int>(data[i]);
    }
    return ss.str();
}

bool ServerClient::configureSearch(const std::string& start_range, const std::string& end_range, 
                                  const std::string& pubkey, int dp_bits, const std::string& range_size) {
    Json::Value request;
    request["start_range"] = start_range;
    request["end_range"] = end_range;
    request["pubkey"] = pubkey;
    request["dp_bits"] = dp_bits;
    request["range_size"] = range_size;
    
    Json::StreamWriterBuilder builder;
    std::string json_str = Json::writeString(builder, request);
    
    HttpResponse response = makePost("/api/configure", json_str);
    
    if (!response.success) {
        std::cerr << "Failed to configure server: HTTP " << response.response_code << std::endl;
        return false;
    }
    
    Json::Value result;
    Json::CharReaderBuilder reader_builder;
    std::string errs;
    std::istringstream iss(response.data);
    
    if (!Json::parseFromStream(reader_builder, iss, &result, &errs)) {
        std::cerr << "Failed to parse configure response: " << errs << std::endl;
        return false;
    }
    
    if (!result["success"].asBool()) {
        std::cerr << "Server configuration failed: " << result["message"].asString() << std::endl;
        return false;
    }
    
    std::cout << "Server configured successfully: " << result["message"].asString() << std::endl;
    return true;
}

std::unique_ptr<ServerWork> ServerClient::getWork() {
    Json::Value request;
    request["client_id"] = client_id;
    
    Json::StreamWriterBuilder builder;
    std::string json_str = Json::writeString(builder, request);
    
    HttpResponse response = makePost("/api/get_work", json_str);
    
    if (!response.success) {
        std::cerr << "Failed to get work from server: HTTP " << response.response_code << std::endl;
        return nullptr;
    }
    
    Json::Value result;
    Json::CharReaderBuilder reader_builder;
    std::string errs;
    std::istringstream iss(response.data);
    
    if (!Json::parseFromStream(reader_builder, iss, &result, &errs)) {
        std::cerr << "Failed to parse work response: " << errs << std::endl;
        return nullptr;
    }
    
    if (!result["success"].asBool()) {
        // No work available
        return nullptr;
    }
    
    auto work = std::make_unique<ServerWork>();
    Json::Value work_data = result["work"];
    
    work->range_id = work_data["range_id"].asString();
    work->start_range = work_data["start_range"].asString();
    work->end_range = work_data["end_range"].asString();
    work->bit_range = work_data["bit_range"].asInt();
    work->dp_bits = work_data["dp_bits"].asInt();
    work->pubkey = work_data["pubkey"].asString();
    
    g_currentRangeId = work->range_id;
    
    std::cout << "Received work assignment: " << work->range_id 
              << " (" << work->start_range << " to " << work->end_range << ")" << std::endl;
    
    return work;
}

bool ServerClient::submitPoints(const std::vector<ServerDP>& points, std::string& response_status, std::string& solution) {
    Json::Value request;
    request["client_id"] = client_id;
    
    Json::Value points_array(Json::arrayValue);
    for (const auto& point : points) {
        Json::Value point_obj;
        point_obj["x_coord"] = point.x_coord;
        point_obj["distance"] = point.distance;
        point_obj["kang_type"] = point.kang_type;
        points_array.append(point_obj);
    }
    request["points"] = points_array;
    
    Json::StreamWriterBuilder builder;
    std::string json_str = Json::writeString(builder, request);
    
    HttpResponse response = makePost("/api/submit_points", json_str);
    
    if (!response.success) {
        std::cerr << "Failed to submit points to server: HTTP " << response.response_code << std::endl;
        return false;
    }
    
    Json::Value result;
    Json::CharReaderBuilder reader_builder;
    std::string errs;
    std::istringstream iss(response.data);
    
    if (!Json::parseFromStream(reader_builder, iss, &result, &errs)) {
        std::cerr << "Failed to parse submit response: " << errs << std::endl;
        return false;
    }
    
    response_status = result["status"].asString();
    
    if (response_status == "solved") {
        solution = result["solution"].asString();
        std::cout << "*** SOLUTION FOUND! ***" << std::endl;
        std::cout << "Solution: " << solution << std::endl;
        return true;
    }
    
    if (result.isMember("points_processed")) {
        std::cout << "Submitted " << result["points_processed"].asInt() << " points to server" << std::endl;
    }
    
    return true;
}

bool ServerClient::getStatus(std::string& status_json) {
    HttpResponse response = makeGet("/api/status");
    
    if (!response.success) {
        std::cerr << "Failed to get server status: HTTP " << response.response_code << std::endl;
        return false;
    }
    
    status_json = response.data;
    return true;
}

bool ServerClient::checkSolved(std::string& solution) {
    std::string status_json;
    if (!getStatus(status_json)) {
        return false;
    }
    
    Json::Value result;
    Json::CharReaderBuilder reader_builder;
    std::string errs;
    std::istringstream iss(status_json);
    
    if (!Json::parseFromStream(reader_builder, iss, &result, &errs)) {
        return false;
    }
    
    if (result["solved"].asBool()) {
        solution = result["solution"].asString();
        return true;
    }
    
    return false;
}

ServerDP ServerClient::convertDP(const u8* dp_data) {
    ServerDP dp;
    
    // Extract x coordinate (first 12 bytes)
    dp.x_coord = bytesToHex(dp_data, 12);
    
    // Extract distance (bytes 16-37, 22 bytes)
    dp.distance = bytesToHex(dp_data + 16, 22);
    
    // Extract kangaroo type (byte 40)
    dp.kang_type = dp_data[40];
    
    return dp;
}