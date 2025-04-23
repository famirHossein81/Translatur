#include <iostream>
#include <string>
#include <curl/curl.h>
#include <sstream>
#include <iomanip>
#include <json.hpp>
#include <fstream>
using json = nlohmann::json;

class Translator
{
public:
    std::string Translate(std::string word);
    void setApiKey(std::string apiKey);
    std::string getApiKey() const;
    void setProxy(std::string ip, std::string port);

private:
    std::string api_key;
    std::string proxy;
};

void Translator::setProxy(std::string ip, std::string port)
{
    proxy = "http://" + ip + ":" + port;
}

void Translator::setApiKey(std::string apiKey)
{
    this->api_key = apiKey;
}

std::string Translator::getApiKey() const
{
    return api_key;
}

size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string *)userp)->append((char *)contents, size * nmemb);
    return size * nmemb;
}

std::string Translator::Translate(std::string word)
{
    CURL *curl = curl_easy_init();
    if (curl)
    {
        std::string api_key = this->api_key;
        std::string url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent?key=" + api_key;
        std::string text = word;
        std::string prompt =
            "Provide Translation the word or sentence(Check which one is it word or sentence) '" + text + "' in the following JSON format:\n"
                                                                                                          "{\n"
                                                                                                          "  \"type\": \"word\",\n"
                                                                                                          "  \"word\": \"" +
            text + "\",\n"
                   "  \"definition\": \"[clear definition]\",\n"
                   "  \"examples\": [\"[example 1]\", \"[example 2]\"],\n"
                   "  \"pronunciation\": \"[IPA pronunciation if available]\",\n"
                   "  \"persian_definition\": \"[Persian translation]\",\n"
                   "  \"synonyms\": [\"[synonym 1]\", \"[synonym 2]\"],\n"
                   "  \"acronym\": \"[full form if acronym, otherwise empty]\"\n"
                   "}\n"
                   "Return only valid JSON.";

        json payload = {
            {"contents", {{{"parts", {{{"text", prompt}}}}}}}};

        std::string json_data = payload.dump();

        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data.c_str());
        struct curl_slist *headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data.c_str());
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_PROXY, proxy.c_str());
        std::string response_string;
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK)
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        else
        {
            json j = json::parse(response_string);
            std::string text_str;
            try
            {
                if (j.contains("candidates") && !j["candidates"].empty())
                {
                    auto &parts = j["candidates"][0]["content"]["parts"];
                    if (!parts.empty() && parts[0].contains("text"))
                    {
                        text_str = parts[0]["text"].get<std::string>();
                        std::cout << "Extracted text:\n"
                                  << text_str << std::endl;
                        const std::string code_block_start = "```json\n";
                        const std::string code_block_end = "\n```";
                        if (text_str.rfind(code_block_start, 0) == 0)
                        {
                            text_str = text_str.substr(code_block_start.length());
                            size_t end_pos = text_str.rfind(code_block_end);
                            if (end_pos != std::string::npos)
                            {
                                text_str = text_str.substr(0, end_pos);
                            }
                        }
                    }
                }
                return text_str;
            }
            catch (const std::exception &e)
            {
                std::cerr << "Error extracting text: " << e.what() << std::endl;
            }
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    return 0;
}