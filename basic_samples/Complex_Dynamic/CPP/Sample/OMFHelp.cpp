#include "OMFHelp.h"

#include <stdio.h>
#include <string>
#include <map>
#include <curl/curl.h>
#include "json/json.h"
#include <iostream>

using namespace std;

map<string, string> getOMFHeaders(string messageType, string action = "create")
{
    map<string, string> headers;
    pair<map<string, string>::iterator, bool> result;

    result = headers.insert(pair<string, string>("messagetype", messageType));
    result = headers.insert(pair<string, string>("action", action));
    result = headers.insert(pair<string, string>("messageformat", "JSON"));
    result = headers.insert(pair<string, string>("omfversion", "1.1"));

    return headers;
}

string authToken = "";
string refreshTime = "";


std::size_t callback(
    const char* in,
    std::size_t size,
    std::size_t num,
    std::string* out)
{
    const std::size_t totalBytes(size * num);
    out->append(in, totalBytes);
    return totalBytes;
}


pair<string, string> getOCSAuthHeader(string clientID, string clientPassword, string urlIn)
{
    CURL* curl;
    CURLcode res;
    curl = curl_easy_init();

    const char* url = urlIn.c_str();
    const char* id = clientID.c_str();
    const char* pass = clientPassword.c_str();

    std::unique_ptr<std::string> httpData(new std::string());
    long httpCode(0);
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_DEFAULT_PROTOCOL, "https");
        struct curl_slist* headers = NULL;
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_mime* mime;
        curl_mimepart* part;
        mime = curl_mime_init(curl);
        part = curl_mime_addpart(mime);
        curl_mime_name(part, "client_id");
        curl_mime_data(part, id, CURL_ZERO_TERMINATED);
        part = curl_mime_addpart(mime);
        curl_mime_name(part, "client_secret");
        curl_mime_data(part, pass, CURL_ZERO_TERMINATED);
        part = curl_mime_addpart(mime);
        curl_mime_name(part, "grant_type");
        curl_mime_data(part, "client_credentials", CURL_ZERO_TERMINATED);
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);


        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, httpData.get());

        res = curl_easy_perform(curl);

        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        curl_mime_free(mime);
    }
    curl_easy_cleanup(curl);

    if (httpCode == 200)
    {
        std::cout << "\nGot successful response from " << url << std::endl;

        // Response looks good - done using Curl now.  Try to parse the results
        // and print them out.
        Json::Value jsonData;

        stringstream str_strm(*httpData.get());

        Json::Value root;   // will contain the root value after parsing.
        Json::CharReaderBuilder builder;
        std::string errs;
        bool ok = Json::parseFromStream(builder, str_strm, &root, &errs);
        if (ok)
        {
            std::cout << "Successfully parsed JSON data" << std::endl;
            std::cout << "\nJSON data received:" << std::endl;
            std::cout << root.toStyledString() << std::endl;

            const std::string access(root["access_token"].asString());
            const std::string expires(root["expires_in"].asString());

            std::cout << "Natively parsed:" << std::endl;
            std::cout << "\taccess: " << access << std::endl;
            std::cout << "\texpires: " << expires << std::endl;
            std::cout << std::endl;

            authToken = "Bearer " + access;
        }
        else
        {
            std::cout << "Could not parse HTTP data as JSON" << std::endl;
            std::cout << "HTTP data was:\n" << *httpData.get() << std::endl;
        }
    }
    else
    {
        std::cout << "Couldn't GET from " << url << " - exiting" << std::endl;
    }
    return pair<string, string>("Authorization", authToken);


}
