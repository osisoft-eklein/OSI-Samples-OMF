#include <stdio.h>
#include <string.h>
#include <curl/curl.h>
#include "json/json.h"
#include <iostream>
#include <fstream>
#include <map>
#include "Sample.h"
using namespace std;

double expirationTime = 0;
string authToken = "";
string refreshTime = "";
time_t authTime = time(NULL);
tm tmNow;

bool ocs = false;
bool eds = false;
bool pi = false;
string omfTypeID = "";
string omfContainerID = "";
string ocsOMFURL = "";
string omfAuth = "";
string OCSclientID = "";
string OCSclientSecret = "";
string piOMFURL = "";
string PIID = "";
string PISecret = "";
string edsOMFURL = "";

CURL* curl;
CURLcode res;

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

pair<string, string> getOCSAuthHeader()
{
    time_t authCheckTime = time(NULL);
    localtime_s(&tmNow, &authCheckTime);
    double seconds = difftime(authCheckTime, authTime);

    if (seconds < expirationTime * .9) {
        return pair<string, string>("Authorization", authToken);
    }

    CURL* curl;
    CURLcode res;

    localtime_s(&tmNow, &authTime);
    curl = curl_easy_init();

    const char* url = omfAuth.c_str();
    const char* id = OCSclientID.c_str();
    const char* pass = OCSclientSecret.c_str();

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
            expirationTime = root["expires_in"].asDouble();

            std::cout << "Natively parsed:" << std::endl;
            std::cout << "\taccess: " << access << std::endl;
            std::cout << "\texpires: " << to_string(expirationTime) << std::endl;
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

void sendMessage(map<string, string> headers, string objectToSend, string url, bool toPI = false) {
  //  curl = curl_easy_init();
    
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_DEFAULT_PROTOCOL, "https");
        if (toPI) {
            curl_easy_setopt(curl, CURLOPT_USERNAME, PISecret.c_str());
            curl_easy_setopt(curl, CURLOPT_PASSWORD, PIID.c_str());
        }

        struct curl_slist* headerList = NULL;
        for (auto const& x : headers)
        {
            string headerName = x.first;
            string headerValue = x.second;
            headerList = curl_slist_append(headerList, (headerName + ": " + headerValue).c_str());
        }
        headerList = curl_slist_append(headerList, "Content-Type: application/json"); //needed for EDS
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
        const char* data = objectToSend.c_str();
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
        /* Perform the request, res will get the return code */
        res = curl_easy_perform(curl);
        /* Check for errors */
        if (res != CURLE_OK)
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));

        long code;
        res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
        if (!res && code)
            printf("The CONNECT response code: %03ld\n", code);

        /* always cleanup */
    //    curl_easy_cleanup(curl);
    }
}

void sendTypes() {

    string typeDef = "[ { \"id\": \"" + omfTypeID + "\", "
        "\"type\": \"object\", \"classification\": \"dynamic\", \"properties\": { "
        "\"Time\": { \"format\": \"date-time\", \"type\": \"string\", \"isindex\": true }, "
        "\"Pressure\": { \"type\": \"number\" }, "
        "\"Temperature\": { \"type\": \"number\" } "
        " } } ]";
    if (ocs) {
        map<string, string> headers = getOMFHeaders("type");
        headers.insert(getOCSAuthHeader());
        sendMessage(headers, typeDef, ocsOMFURL);
    }
    if(eds) {
        map<string, string> headers = getOMFHeaders("type");
        sendMessage(headers, typeDef, edsOMFURL);
    }
    if (pi) {
        map<string, string> headers = getOMFHeaders("type");
        sendMessage(headers, typeDef, piOMFURL, true);
    }
}

void sendContainers() {
    string containerDef = "[ { \"id\": \"" + omfContainerID + "\", "
        "\"typeid\": \"" + omfTypeID + "\" } ]";
    if (ocs) {
        map<string, string> headers = getOMFHeaders("container");
        headers.insert(getOCSAuthHeader());
        sendMessage(headers, containerDef, ocsOMFURL);
    }
    if (eds) {
        map<string, string> headers = getOMFHeaders("container");
        sendMessage(headers, containerDef, edsOMFURL);
    }
    if (pi) {
        map<string, string> headers = getOMFHeaders("container");
        sendMessage(headers, containerDef, piOMFURL, true);
    }
}

string getTimestampString()
{

    time_t ttNow = time(NULL);
    tm tmNow;
    localtime_s(&tmNow, &ttNow);
    char s[256];
    strftime(s, sizeof(s), "%Y-%m-%dT%H:%M:%SZ", &tmNow);
    return (string)s;
}

void sendData(double temp, double pressure) {

    string timestamp = getTimestampString();
    string dataDef = "[ { \"containerid\": \"" + omfContainerID + "\", \"values\": [ { "
        "\"Time\": \"" + timestamp + "\"," +
        "\"Temperature\": " + to_string(temp) + ", "
        "\"Pressure\": " + to_string(pressure) + "  } ] } ]";
    if (ocs) {
        map<string, string> headers = getOMFHeaders("data");
        headers.insert(getOCSAuthHeader());
        sendMessage(headers, dataDef, ocsOMFURL);
    }
    if (eds) {
        map<string, string> headers = getOMFHeaders("data");
        sendMessage(headers, dataDef, edsOMFURL);
    }
    if (pi) {
        map<string, string> headers = getOMFHeaders("data");
        sendMessage(headers, dataDef, piOMFURL, true);
    }
}

int main(void)
{
    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();


    //I would pull down the config from OCS.  Do a simple GET with auth (I would use a different auth for this, allowing you to change the values for sending and allows security to be locked down).


    Json::Value root;   // will contain the root value after parsing.
    Json::CharReaderBuilder builder;
    std::ifstream test("config.json", std::ifstream::binary);
    std::string errs;
    bool ok = Json::parseFromStream(builder, test, &root, &errs);
    ocs = root["ocs"].asBool();
    eds = root["eds"].asBool();
    pi = root["pi"].asBool();
    omfTypeID = root["omfTypeID"].asString();
    omfContainerID = root["omfContainerID"].asString();
    ocsOMFURL = root["ocsOMFURL"].asString();
    omfAuth = root["omfAuth"].asString();
    OCSclientID = root["OCSclientID"].asString();
    OCSclientSecret = root["OCSclientSecret"].asString();
    piOMFURL = root["piOMFURL"].asString();
    PIID = root["PIID"].asString();
    PISecret = root["PISecret"].asString();
    edsOMFURL = root["edsOMFURL"].asString();

    sendTypes();
    sendContainers();
    sendData(3,5);
    curl_easy_cleanup(curl);
       
    return 0;
}

