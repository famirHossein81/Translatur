#pragma once
#include <iostream>
#include <string>
#include <curl/curl.h>
#include <sstream>
#include <iomanip>
#include <json.hpp>
#include <fstream>

class Translator
{
public:
    std::string Translate(std::string word);
    void setApiKey(std::string api_key);
    std::string getApiKey() const;
    void setProxy(std::string ip, std::string port);

private:
    std::string api_key;
    std::string proxy;
};
