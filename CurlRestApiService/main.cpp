/***************************************************************************
*                                  _   _ ____  _
*  Project                     ___| | | |  _ \| |
*                             / __| | | | |_) | |
*                            | (__| |_| |  _ <| |___
*                             \___|\___/|_| \_\_____|
*
* Copyright (C) 1998 - 2015, Daniel Stenberg, <daniel@haxx.se>, et al.
*
* This software is licensed as described in the file COPYING, which
* you should have received as part of this distribution. The terms
* are also available at https://curl.haxx.se/docs/copyright.html.
*
* You may opt to use, copy, modify, merge, publish, distribute and/or sell
* copies of the Software, and permit persons to whom the Software is
* furnished to do so, under the terms of the COPYING file.
*
* This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
* KIND, either express or implied.
*
***************************************************************************/
/* <DESC>
* Simple HTTPS GET
* </DESC>
*/
#include <stdio.h>
#include <curl/curl.h>

int main(void)
{
  CURL *curl;
  CURLcode res;

  curl_global_init(CURL_GLOBAL_DEFAULT);

  curl = curl_easy_init();
  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, "https://repository.iqrfalliance.org/api/server/");

#define SKIP_PEER_VERIFICATION
#ifdef SKIP_PEER_VERIFICATION
    /*
    * If you want to connect to a site who isn't using a certificate that is
    * signed by one of the certs in the CA bundle you have, you can skip the
    * verification of the server's certificate. This makes the connection
    * A LOT LESS SECURE.
    *
    * If you have a CA cert for the server stored someplace else than in the
    * default bundle, then the CURLOPT_CAPATH option might come handy for
    * you.
    */
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
#endif

#ifdef SKIP_HOSTNAME_VERIFICATION
    /*
    * If the site you're connecting to uses a different host name that what
    * they have mentioned in their server certificate's commonName (or
    * subjectAltName) fields, libcurl will refuse to connect. You can skip
    * this check, but this will make the connection less secure.
    */
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
#endif

    /* Perform the request, res will get the return code */
    res = curl_easy_perform(curl);
    /* Check for errors */
    if (res != CURLE_OK)
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
        curl_easy_strerror(res));

    /* always cleanup */
    curl_easy_cleanup(curl);
  }

  curl_global_cleanup();

  return 0;
}


#if 0
#include <cpprest/http_client.h>
#include <cpprest/filestream.h>
#include <cpprest/http_listener.h>              // HTTP WsServer
#include <cpprest/json.h>                       // JSON library
#include <cpprest/uri.h>                        // URI library
#include <cpprest/ws_client.h>                  // WebSocket client
#include <cpprest/containerstream.h>            // Async streams backed by STL containers
#include <cpprest/interopstream.h>              // Bridges for integrating Async streams with STL and WinRT streams
#include <cpprest/rawptrstream.h>               // Async streams backed by raw pointer to memory
#include <cpprest/producerconsumerstream.h>     // Async streams for producer consumer scenarios

using namespace utility;                    // Common utilities like string conversions
using namespace web;                        // Common features like URIs.
using namespace web::http;                  // Common HTTP functionality
using namespace web::http::client;          // HTTP client features
using namespace concurrency::streams;       // Asynchronous streams

int main(int argc, char* argv[])
{
  auto fileStream = std::make_shared<ostream>();

  // Open stream to output file.
  pplx::task<void> requestTask = fstream::open_ostream(U("results.html")).then([=](ostream outFile)
  {
    *fileStream = outFile;

    // Create http_client to send the request.
    //http_client client(U("https://repository.iqrfalliance.org/api/server/"));
    auto url = U("https://www.bing.com/");
    http_client client(url);

    // Build request URI and start the request.
    //uri_builder builder(U("/search"));
    //builder.append_query(U("q"), U("cpprestsdk github"));
    //return client.request(methods::GET, builder.to_string());
    printf("Getting: %ws\n", url);
    return client.request(methods::GET);
  })

    // Handle response headers arriving.
    .then([=](http_response response)
  {
    printf("Received response status code:%u\n", response.status_code());

    // Write response body into the file.
    return response.body().read_to_end(fileStream->streambuf());
  })

    // Close the file stream.
    .then([=](size_t)
  {
    return fileStream->close();
  });

  // Wait for all the outstanding I/O to complete and handle any exceptions
  try
  {
    requestTask.wait();
  }
  catch (const std::exception &e)
  {
    printf("Error exception:%s\n", e.what());
  }

  return 0;
}
#endif
