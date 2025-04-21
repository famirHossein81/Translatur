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

private:
    std::string api_key;
};
