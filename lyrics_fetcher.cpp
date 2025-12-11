/*
 * Copyright 12/07/2025 https://github.com/su8/0verau
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */
#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <curl/curl.h>
#include "json.hpp"

using json = nlohmann::json;

// Callback function to write received data into a string
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
  size_t totalSize = size * nmemb;
  std::string* str = static_cast<std::string*>(userp);
  str->append(static_cast<char*>(contents), totalSize);
  return totalSize;
}

bool fetchLyricsToFile(const std::string& url, const std::string& outputFile) {
  CURL* curl;
  CURLcode res;
  std::string response;
  long int http_code = 0;

  curl_global_init(CURL_GLOBAL_DEFAULT);
  curl = curl_easy_init();

  if (!curl) {
    std::cerr << "Failed to initialize cURL.\n";
    curl_global_cleanup();
    return false;
  }

  // Set cURL options
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // Follow redirects
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "Chrome Fetcher/1.0");

  // Perform the request
  res = curl_easy_perform(curl);

  if (res != CURLE_OK) {
    std::cerr << "cURL error: " << curl_easy_strerror(res) << std::endl;
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return false;
  }

  if (res == CURLE_OK) {
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code == 404) {
      return false;
    }
  }


  // Save to .lrc file
  std::ofstream outFile(outputFile, std::ios::out | std::ios::trunc);
  if (!outFile) {
    std::cerr << "Error opening file for writing: " << outputFile << std::endl;
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return false;
  }
  outFile << response;
  outFile.close();

  std::cout << "Lyrics saved to: " << outputFile << std::endl;

  // Cleanup
  curl_easy_cleanup(curl);
  curl_global_cleanup();
  return true;
}

int main(int argc, char *argv[]) {
  //std::string apiUrl = "https://lrclib.net/api/get?artist_name=50 Cent - Ayo Technology (Official Music Video) ft. Justin Timberlake&album_name=Unknown Album&track_name=50 Cent - Ayo Technology (Official Music Video) ft. Justin Timberlake";
  if (argc < 3) { std::cerr << argv[0] << " expected 'url' 'file.lrc'" << std::endl; return EXIT_FAILURE; }
  std::string apiUrl = argv[1];
  std::string outputFile = argv[2];

  if (!fetchLyricsToFile(apiUrl, outputFile)) {
    std::cerr << "Failed to fetch lyrics.\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}