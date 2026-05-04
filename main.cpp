#include "http/http.h"
#include "traffic/traffic.h"
#include <iostream>
#include <sstream>
#include <iomanip>

int main() {
    try {
        TrafficTester tester;

        std::string url    = "https://jsonplaceholder.typicode.com/posts";
        int totalRequests  = 10;
        int threadCount    = 4;

        std::cout << "Launching " << threadCount
                  << " threads x " << (totalRequests / threadCount)
                  << " requests each (thundering herd)\n\n";

        TrafficResult r = tester.run("GET", url, totalRequests, threadCount,
                                     /*body=*/"", /*thunderingHerd=*/true);

        auto pct = [](double v) -> std::string {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2) << v;
            return oss.str();
        };

        std::cout << "---------------------------------------\n";
        std::cout << "Total requests  : " << r.totalRequests    << "\n";
        std::cout << "Success         : " << r.successRequests  << "\n";
        std::cout << "Failed          : " << r.failedRequests   << "\n";
        std::cout << "---------------------------------------\n";
        std::cout << "Total time (ms) : " << pct(r.totalTimeMs)      << "\n";
        std::cout << "RPS             : " << pct(r.requestsPerSecond) << "\n";
        std::cout << "---------------------------------------\n";
        std::cout << "Latency (ms)\n";
        std::cout << "  min  : " << pct(r.minLatencyMs) << "\n";
        std::cout << "  avg  : " << pct(r.avgLatencyMs) << "\n";
        std::cout << "  p50  : " << pct(r.p50LatencyMs) << "\n";
        std::cout << "  p95  : " << pct(r.p95LatencyMs) << "\n";
        std::cout << "  p99  : " << pct(r.p99LatencyMs) << "\n";
        std::cout << "  max  : " << pct(r.maxLatencyMs) << "\n";
        std::cout << "---------------------------------------\n";

    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
    }

    std::cin.get();
}