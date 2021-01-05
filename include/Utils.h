#pragma once

#include <string>


namespace Utils
{
    bool IsConsonant(char ch);
    void Reverse(std::string& str);

    template <class ContainerT>
    void Split(const std::string& str, ContainerT& cont, char delim = ' ')
    {
        size_t current, previous = 0;
        current = str.find(delim);
        while (current != std::string::npos) {
            cont.push_back(str.substr(previous, current - previous));
            previous = current + 1;
            current = str.find(delim, previous);
        }
        cont.push_back(str.substr(previous, current - previous));
    };
}
