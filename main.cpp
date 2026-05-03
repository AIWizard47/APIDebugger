#include "http\http.h"
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
        std::cout << c.GET(url) << "\n";
    }
    catch (const std::exception& e) {
        std::cout<<"Error: ";
        std::cerr << e.what() << "\n";
    }
    std::cin.get();
    return 0;
}