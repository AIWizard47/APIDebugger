#pragma once

#include <string>
#include <vector>
#include <mutex>

class HTTPClient;

struct TrafficResult
{
    int totalRequests = 0;
    int successRequests = 0;
    int failedRequests = 0;

    double totalTimeMs = 0;

    double avgLatencyMs = 0;
    double minLatencyMs = 0;
    double maxLatencyMs = 0;

    double requestsPerSecond = 0;
};

class TrafficTester
{
private:

    std::mutex mtx;

    std::vector<double> latencies;

    int successCount = 0;

    int failCount = 0;

    void worker(
        const std::string& method,
        const std::string& url,
        const std::string& body,
        int requestCount
    );

public:

    TrafficResult run(
        HTTPClient& client,
        const std::string& method,
        const std::string& url,
        int totalRequests,
        int threadCount,
        const std::string& body = ""
    );
};