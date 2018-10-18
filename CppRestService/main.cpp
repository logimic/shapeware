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
    http_client client(U("https://repository.iqrfalliance.org/api/server/"));

    // Build request URI and start the request.
    //uri_builder builder(U("/search"));
    //builder.append_query(U("q"), U("cpprestsdk github"));
    //return client.request(methods::GET, builder.to_string());
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