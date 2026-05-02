#include "http\http.h"
#include <iostream>

int main() {
    try {
        HTTPClient c;

        std::cout << "---- HTTP ----\n";
        std::cout << c.GET("http://example.com") << "\n\n";

        std::cout << "---- HTTPS ----\n";
        std::cout << c.GET("https://jsonplaceholder.typicode.com/posts") << "\n";
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
    }
}