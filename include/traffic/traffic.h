// traffic/traffic.h
#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <atomic>

class PersistentHTTPClient;

struct TrafficResult {
    int    totalRequests    = 0;
    int    successRequests  = 0;
    int    failedRequests   = 0;
    double totalTimeMs      = 0;
    double avgLatencyMs     = 0;
    double minLatencyMs     = 0;
    double maxLatencyMs     = 0;
    double p50LatencyMs     = 0;   // median
    double p95LatencyMs     = 0;   // 95th percentile
    double p99LatencyMs     = 0;   // 99th percentile — shows tail pain
    double requestsPerSecond= 0;
};

class TrafficTester {
public:
    // thundering = true  → all threads fire simultaneously (default)
    // thundering = false → staggered ramp-up (gentler load)
    TrafficResult run(
        const std::string& method,
        const std::string& url,
        int   totalRequests,
        int   threadCount,
        const std::string& body       = "",
        bool  thunderingHerd          = true
    );

private:
    std::mutex          mtx_;
    std::vector<double> latencies_;
    std::atomic<int>    successCount_{0};
    std::atomic<int>    failCount_{0};

    void worker(
        const std::string& method,
        const std::string& url,
        const std::string& path,      // pre-parsed
        const std::string& body,
        int   requestCount,
        std::atomic<int>& readyCount, // barrier counter
        int   totalThreads,
        bool  thunderingHerd
    );

    static double percentile(std::vector<double>& sorted, double p);
};