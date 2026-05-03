#include "http\http.h"
#include <chrono>
#include <iostream>

int main(int argc, char* argv[]) {
    try {
        HTTPClient c;
        std::string url;
        // There is total numer of argument is 2 but indexing is start from 0 to its 0,1 total.
        if (argc<2)
        {
            url = "https://example.com/";
        }
        else
        {
            url = argv[1];
        }
        std::cout<<"Number of command : "<<argc<<"\n";
        std::cout << "---- HTTPS ----\n";
        auto start = std::chrono::high_resolution_clock::now(); // Get current time point
        // ... code to measure ...
        std::string res = c.GET(url);
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start; // Difference
        std::cout << res << "\n";
        std::cout << "Waited: " << elapsed.count() << "ms\n";


        std::cout << "---- POST HTTPS ----\n";
        std::string json =
            R"({"title":"Fuck","body":"you","userId":1})";
        start = std::chrono::high_resolution_clock::now(); // Get current time point
        // ... code to measure ...
        std::string response =
            c.POST(
                url,
                json
            );
        end = std::chrono::high_resolution_clock::now();
        elapsed = end - start; // Difference
        std::cout << response << std::endl;
        std::cout << "Waited: " << elapsed.count() << "ms\n";

    }
    catch (const std::exception& e) {
        std::cout<<"Error: ";
        std::cerr << e.what() << "\n";
    }
    std::cin.get();
    return 0;
}