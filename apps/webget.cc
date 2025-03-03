#include "exception.hh"
#include "socket.hh"

#include <cstdlib>
#include <iostream>
#include <span>
#include <string>

using namespace std;

void get_URL( const string& host, const string& path )
{

  if ( host.empty() || path.empty() ) {
    throw runtime_error( "Empty host or path" );
  }

  try {
    TCPSocket tcp;

    // Connect to host
    try {
      tcp.connect( { host, "http" } );
    } catch ( const unix_error& e ) {
      throw unix_error( "Failed to connect to " + host + ": " + e.what() );
    }

    // Format and send HTTP request
    const string request = "GET " + path
                           + " HTTP/1.0\r\n"
                             "Host: "
                           + host
                           + "\r\n"
                             "Connection: close\r\n"
                             "\r\n";
    try {
      tcp.write( request );
      tcp.shutdown( SHUT_WR ); // 关闭写入方向，表明请求发送完毕
    } catch ( const unix_error& e ) {
      throw unix_error( "Failed to send request to " + host + ": " + e.what() );
    }

    // Read and print response
    try {
      constexpr size_t BUFFER_SIZE = 1024;
      string response;
      response.reserve( BUFFER_SIZE );

      while ( !tcp.eof() ) {
        tcp.read( response );
        if ( !response.empty() ) {
          cout << response;
          cout.flush();
        }
      }
    } catch ( const unix_error& e ) {
      throw unix_error( "Failed to read response from " + host + ": " + e.what() );
    }

  } catch ( const exception& e ) {
    cerr << "Error: " << e.what() << "\n";
    throw;
  }
}

int main( int argc, char* argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort(); // For sticklers: don't try to access argv[0] if argc <= 0.
    }

    auto args = span( argv, argc );

    // The program takes two command-line arguments: the hostname and "path" part of the URL.
    // Print the usage message unless there are these two arguments (plus the program name
    // itself, so arg count = 3 in total).
    if ( argc != 3 ) {
      cerr << "Usage: " << args.front() << " HOST PATH\n";
      cerr << "\tExample: " << args.front() << " stanford.edu /class/cs144\n";
      return EXIT_FAILURE;
    }

    // Get the command-line arguments.
    const string host { args[1] };
    const string path { args[2] };

    // Call the student-written function.
    get_URL( host, path );
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
