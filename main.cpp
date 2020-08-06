#include <iostream>
#include <iomanip>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio.hpp>
#include <boost/property_tree/ptree.hpp>
#include "symbols.hpp"

/*
  Run this for at least 5 minutes (piping stdout to a file)
  grep for the string "issue_occured=1" in stdout - that is what I claim is a bug

  See README ror more information about the context of the issue
 */

long ONE_SECOND = 1000000000L;

long get_time(){
  return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

int main(){
  
  const std::string host = "api.zb.cn"; //I picked this exchange because they have huge message sizes, and huge message sizes make the issue more likely to occur
  const std::string url = "/websocket";
  const int port = 443;

  boost::asio::io_context ioc;
  boost::asio::ip::tcp::resolver resolver{ioc};
  boost::asio::ssl::context ctx{boost::asio::ssl::context::sslv23_client};
  boost::beast::websocket::stream<boost::asio::ssl::stream< boost::asio::ip::tcp::socket>> ws{ioc, ctx};
  const boost::asio::basic_stream_socket<boost::asio::ip::tcp>& socket = ws.next_layer().next_layer();
  auto const results = resolver.resolve(boost::asio::ip::tcp::v4(), host, std::to_string(port));
  
  ws.next_layer().next_layer().connect(*results);
  
  ws.next_layer().handshake(boost::asio::ssl::stream_base::client);

  ws.handshake(host, url);

  for(const std::string& symbol: SYMBOLS) {
    for(const std::string& feed_type: {"trades", "depth"}) {
      ws.write(boost::asio::buffer("{\"event\":\"addChannel\",\"channel\":\"" + symbol + "_" + feed_type + "\"}"));
    }
  }
  
  boost::beast::flat_buffer buffer;
  
  while(true){

    const auto pre_read_available = socket.available();
    const long pre_read_time = get_time();    
    ws.read(buffer);
    const long post_read_time = get_time();    
    const auto post_read_available = socket.available();
    
    const std::string_view message_view{ //this minimal example doesn't use this (aside from the size), but I'm including it here in case anyone wants to inspect it
      reinterpret_cast<const char*>(boost::beast::buffers_front(buffer.data()).data()),
	boost::beast::buffers_front(buffer.data()).size()};

    const long read_time_micros = (post_read_time - pre_read_time)/1000;
    
    const bool issue_occured = //if there was plenty of data to read, then we should have read it quickly!
      pre_read_available > 2 * message_view.size() && //the 2 here is abritary. I think that 1 is the correct threshold, but I wanted to be safe (so people don't say "headers", "compression", etc)
      read_time_micros > 2000;
    
    std::cout <<
      "issue_occured=" << issue_occured << ", " <<
      "read_time_micros=" << std::setw(9) << read_time_micros << ", " <<
      "pre_read_available=" << std::setw(9) << pre_read_available << ", " <<
      "post_read_available=" << std::setw(9) << post_read_available << ", " <<
      "message_size=" << message_view.size() << std::endl;
    
    buffer.consume(message_view.size());
  }
}

