#include "Utils.h"


namespace Utils
{
    bool IsConsonant(char ch) 
    {
        int lowCh = tolower(ch);
        return (isalpha(ch) && lowCh != 'a' && lowCh != 'e' && lowCh != 'i' && lowCh != 'o' && lowCh != 'u');
    }

    void Reverse(std::string& str)
    {
        size_t n = str.length();
    
        for (size_t i = 0; i < n/2; ++i) {
            std::swap(str[i], str[n-i-1]);
        }
    }
}
