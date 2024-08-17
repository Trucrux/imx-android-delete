// Copyright 2010 Dean Michael Berris.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include "network/file-server.hpp"
#include "common/filesystem.hpp"
#include "common/json-app.hpp"
#include "shell/api.hpp"
#include "shell/shell.hpp"
#include <boost/asio.hpp>
#include <boost/network/protocol/http/server.hpp>
#include <boost/network/utils/thread_pool.hpp>
#include <boost/shared_ptr.hpp>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

namespace http = boost::network::http;
namespace utils = boost::network::utils;

struct file_server;
typedef http::server<file_server> server;

struct file_cache {
  typedef std::map<std::string, std::pair<void *, std::size_t>> region_map;
  typedef std::map<std::string, std::vector<server::response_header>> meta_map;

  std::string doc_root_;
  region_map regions;
  meta_map file_headers;
  std::mutex cache_mutex;

  explicit file_cache(std::string doc_root) : doc_root_(std::move(doc_root)) {}

  ~file_cache() throw() {
    for (auto &region : regions) {
      munmap(region.second.first, region.second.second);
    }
  }

  bool has(std::string const &path) {
    std::unique_lock<std::mutex> lock(cache_mutex);
    return regions.find(doc_root_ + path) != regions.end();
  }

  bool add(std::string const &path) {
    std::unique_lock<std::mutex> lock(cache_mutex);
    std::string real_filename = doc_root_ + path;

    if (regions.find(path) != regions.end()) {
      return true;
    }

    std::pair<void *, std::size_t> region = mmapFile(real_filename);
    if (region.first == MAP_FAILED || !region.second) {
      return false;
    }

    regions.insert(std::make_pair(real_filename, region));
    file_headers.insert(
        std::make_pair(real_filename, createHeaders(region.second)));

    return true;
  }

  std::vector<server::response_header> createHeaders(off_t size) {
    static server::response_header common_headers[] = {
        {"Connection", "close"},
        {"Content-Type", "x-application/octet-stream"},
        {"Content-Length", "0"}};
    std::vector<server::response_header> headers(common_headers,
                                                 common_headers + 3);
    headers[2].value = std::to_string(size);

    return headers;
  }

  std::pair<void *, std::size_t> get(std::string const &path) {
    std::unique_lock<std::mutex> lock(cache_mutex);
    region_map::const_iterator region = regions.find(doc_root_ + path);
    if (region != regions.end())
      return region->second;
    else
      return std::pair<void *, std::size_t>(0, 0);
  }

  boost::iterator_range<std::vector<server::response_header>::iterator>
  meta(std::string const &path) {
    std::unique_lock<std::mutex> lock(cache_mutex);
    static std::vector<server::response_header> empty_vector;
    auto headers = file_headers.find(doc_root_ + path);
    if (headers != file_headers.end()) {
      auto begin = headers->second.begin(), end = headers->second.end();
      return boost::make_iterator_range(begin, end);
    } else
      return boost::make_iterator_range(empty_vector);
  }

  std::pair<void *, std::size_t> mmapFile(std::string const &path) {
#ifdef O_NOATIME
    int32_t fd = open(path.c_str(), O_RDONLY | O_NOATIME | O_NONBLOCK);
#else
    int32_t fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
#endif
    if (fd == -1) {
      return std::pair<void *, std::size_t>(MAP_FAILED, 0);
    }

    off_t size = lseek(fd, 0, SEEK_END);
    if (size == -1) {
      close(fd);
      return std::pair<void *, std::size_t>(MAP_FAILED, 0);
    }

    void *region = mmap(0, size, PROT_READ, MAP_PRIVATE, fd, 0);

    close(fd);

    return std::make_pair(region, size);
  }

  bool isEnable = false;
};

struct connection_handler : std::enable_shared_from_this<connection_handler> {
  explicit connection_handler(file_cache &cache) : file_cache_(cache) {}

  void operator()(std::string const &path, server::connection_ptr connection,
                  bool serve_body) {
    if (file_cache_.isEnable) {
      sendFileWithCached(path, connection, serve_body);
    } else {
      sendFileWithoutCached(path, connection, serve_body);
    }
  }

  void not_found(std::string const &, server::connection_ptr connection) {
    static server::response_header headers[] = {{"Connection", "close"},
                                                {"Content-Type", "text/plain"}};
    connection->set_status(server::connection::not_found);
    connection->set_headers(boost::make_iterator_range(headers, headers + 2));
    connection->write("File Not Found!");
    std::cout << "File Not Found!" << std::endl;
  }

  template <class Range>
  void send_headers(Range const &headers, server::connection_ptr connection) {
    connection->set_status(server::connection::ok);
    connection->set_headers(headers);
  }

  void sendFileWithCached(std::string const &path,
                          server::connection_ptr connection, bool serve_body) {
    bool ok = file_cache_.has(path);
    if (!ok)
      ok = file_cache_.add(path);
    if (ok) {
      send_headers(file_cache_.meta(path), connection);
      if (serve_body) {
        reportInfo(path);

        send_file(file_cache_.get(path), 0, connection);
      }
    } else {
      not_found(path, connection);
    }
  }

  void sendFileWithoutCached(std::string const &path,
                             server::connection_ptr connection,
                             bool serve_body) {
    std::cout << "path: " << path << std::endl;
    std::string real_filename = file_cache_.doc_root_ + path;
    std::cout << "real_filename: " << real_filename << std::endl;

    std::pair<void *, std::size_t> region = file_cache_.mmapFile(real_filename);
    if (region.first != MAP_FAILED) {
      send_headers(file_cache_.createHeaders(region.second), connection);

      if (serve_body) {
        reportInfo(path);

        send_file(region, 0, connection);
      }
    } else {
      not_found(path, connection);
    }
  }

  void send_file(std::pair<void *, std::size_t> mmaped_region, off_t offset,
                 server::connection_ptr connection) {
    // chunk it up page by page
    std::size_t adjusted_offset = offset + 4096;
    off_t rightmost_bound = std::min(mmaped_region.second, adjusted_offset);
    auto self = this->shared_from_this();
    connection->write(
        boost::asio::const_buffers_1(
            static_cast<char const *>(mmaped_region.first) + offset,
            rightmost_bound - offset),
        [=](boost::system::error_code const &ec) {
          self->handle_chunk(mmaped_region, rightmost_bound, connection, ec);
        });
  }

  void handle_chunk(std::pair<void *, std::size_t> mmaped_region, off_t offset,
                    server::connection_ptr connection,
                    boost::system::error_code const &ec) {
    assert(offset >= 0);
    if (!ec && static_cast<std::size_t>(offset) < mmaped_region.second) {
      send_file(mmaped_region, offset, connection);
    } else {
      if (!file_cache_.isEnable) {
        assert(!munmap(mmaped_region.first, mmaped_region.second));
      }
    }
  }

  void reportInfo(std::string const &path) {
    std::cout << "<<<" << std::endl;
    std::cout << "Download file" << std::endl;
    std::cout << "===" << std::endl;
    std::cout << path.c_str() << std::endl;
    std::cout << ">>>" << std::endl;
  }

  file_cache &file_cache_;
};

struct input_consumer : public std::enable_shared_from_this<input_consumer> {
  // Maximum size for incoming request bodies.
  static constexpr std::size_t MAX_INPUT_BODY_SIZE = 2 << 16;

  explicit input_consumer(std::shared_ptr<connection_handler> h,
                          server::request r, void *pUserContext)
      : request_(std::move(r)), handler_(std::move(h)), content_length_{0},
        pUserContext(pUserContext) {
    for (const auto &header : request_.headers) {
      if (boost::iequals(header.name, "content-length")) {
        content_length_ = std::stoul(header.value);
        // std::cout << "Content length: " << content_length_ << '\n';
      } else if (boost::iequals(header.name, "content-type")) {
        contentType = header.value;
        // std::cout << "Content type: " << contentType << '\n';
      }
    }
  }

  void operator()(server::connection::input_range input,
                  boost::system::error_code ec,
                  std::size_t /*bytes_transferred*/,
                  server::connection_ptr connection) {
    // std::cout << "Callback: " << bytes_transferred << "; ec = " << ec <<
    // '\n';
    if (ec == boost::asio::error::eof)
      return;
    if (!ec) {
      if (empty(input)) {
        std::cout << "connection::input is empty" << std::endl;
        return (*handler_)(request_.destination, connection, true);
      }

      request_.body.insert(request_.body.end(), boost::begin(input),
                           boost::end(input));
      if (request_.body.size() > MAX_INPUT_BODY_SIZE) {
        connection->set_status(server::connection::bad_request);
        static server::response_header error_headers[] = {
            {"Connection", "close"}};
        connection->set_headers(
            boost::make_iterator_range(error_headers, error_headers + 1));
        connection->write("Body too large.");
        return;
      }

      if (request_.body.size() == content_length_) {
        if (boost::iequals(contentType, "application/json")) {
          return handle_json(request_.body, connection);
        } else {
          std::cerr << "Body: " << request_.body << '\n';
          return (*handler_)(request_.destination, connection, true);
        }
      }

      std::cerr << "Scheduling another read...\n";
      auto self = this->shared_from_this();
      connection->read([self](
          server::connection::input_range input, boost::system::error_code ec,
          std::size_t bytes_transferred, server::connection_ptr connection) {
        (*self)(input, ec, bytes_transferred, connection);
      });
    }
  }

  void handle_json(std::string body, server::connection_ptr connection) {
    std::ostringstream data;

    std::cout << "<<<" << std::endl;
    std::cout << body << std::endl;
    std::cout << "===" << std::endl;

    try {
      Json::Value jQuery = JsonApp::fromString(body);
      Json::Value jResponse;

      sh::ShellApi().process(jQuery, jResponse);

      data << JsonApp::toString(jResponse, "");
    } catch (const std::exception &e) {
      Json::Value jResponse;

      jResponse[REST_RET] = RET_INVALID_PARM;
      jResponse[REST_MSG] = e.what();

      data << JsonApp::toString(jResponse, "");
    }

    std::map<std::string, std::string> headers = {
        {"Content-Length", "0"}, {"Content-Type", "application/json"},
    };

    auto responseBody = data.str();

    headers["Content-Length"] = std::to_string(responseBody.size());

    connection->set_status(server::connection::ok);
    connection->set_headers(headers);
    connection->write(responseBody);

    std::cout << responseBody << std::endl;
    std::cout << ">>>" << std::endl;
  }

  server::request request_;
  std::shared_ptr<connection_handler> handler_;
  size_t content_length_;
  std::string contentType;

  void *pUserContext;
};

///
/// Custom exception type
///
struct file_uploader_exception : public std::runtime_error {
  file_uploader_exception(const std::string err) : std::runtime_error(err) {}
};

///
/// Encapsulates request & connection
///
struct file_uploader : std::enable_shared_from_this<file_uploader> {
  const server::request &request;
  server::connection_ptr connection;

  std::mutex mtx;
  std::condition_variable condvar;

  std::string filename;

  FILE *fp = NULL;

public:
  file_uploader(const server::request &request,
                const server::connection_ptr &connection)
      : request(request), connection(connection) {
    const std::string dest = destination(request);

    if (dest.find("/upload") != std::string::npos) {
      auto queries = get_queries(dest);

      std::string path;

      auto pname = queries.find("path");
      if (pname != queries.end()) {
        path = pname->second;

        fs::mkdirP(path);
      }

      auto fname = queries.find("filename");
      if (fname != queries.end()) {
        filename = fname->second;

        if (!path.empty()) {
          filename = path + "/" + filename;
        }

        fp = ::fopen(filename.c_str(), "wb");
        if (!fp) {
          throw file_uploader_exception("Failed to open file to write");
        }
      } else {
        throw file_uploader_exception("'filename' cannot be empty");
      }
    }
  }

  ~file_uploader() {
    if (fp) {
      std::cout << "<<<" << std::endl;
      std::cout << filename << std::endl;
      std::cout << "===" << std::endl;
      std::cout << "Upload file, size = " << ftell(fp) << std::endl;
      std::cout << "<<<" << std::endl;
      ::fflush(fp);
      ::fclose(fp);
    }
  }

  ///
  /// Non blocking call to initiate the data transfer
  ///
  void async_recv() {
    std::size_t content_length = 0;
    auto const &headers = request.headers;
    for (auto item : headers) {
      if (boost::to_lower_copy(item.name) == "content-length") {
        content_length = std::stoll(item.value);
        break;
      }
    }

    read_chunk(connection, content_length);
  }

  ///
  /// The client shall wait by calling this until the transfer is done by
  /// the IO threadpool
  ///
  void wait_for_completion() {
    std::unique_lock<std::mutex> _(mtx);
    condvar.wait(_);
  }

private:
  ///
  /// Parses the string and gets the query as a key-value pair
  ///
  /// @param [in] dest String containing the path and the queries, without the
  /// fragment,
  ///                  of the form "/path?key1=value1&key2=value2"
  ///
  std::map<std::string, std::string> get_queries(const std::string dest) {

    std::size_t pos = dest.find_first_of("?");

    std::map<std::string, std::string> queries;
    if (pos != std::string::npos) {
      std::string query_string = dest.substr(pos + 1);

      // Replace '&' with space
      for (pos = 0; pos < query_string.size(); pos++) {
        if (query_string[pos] == '&') {
          query_string[pos] = ' ';
        }
      }

      std::istringstream sin(query_string);
      while (sin >> query_string) {

        pos = query_string.find_first_of("=");

        if (pos != std::string::npos) {
          const std::string key = query_string.substr(0, pos);
          const std::string value = query_string.substr(pos + 1);
          queries[key] = value;
        }
      }
    }

    return queries;
  }

  ///
  /// Reads a chunk of data
  ///
  /// @param [in] connection        Connection to read from
  /// @param [in] left2read   Size to read
  ///
  void read_chunk(server::connection_ptr connection, std::size_t left2read) {
    connection->read(boost::bind(
        &file_uploader::on_data_ready, file_uploader::shared_from_this(),
        boost::placeholders::_1, boost::placeholders::_2,
        boost::placeholders::_3, connection, left2read));
  }

  ///
  /// Callback that gets called when the data is ready to be consumed
  ///
  void on_data_ready(server::connection::input_range range,
                     boost::system::error_code error, std::size_t size,
                     server::connection_ptr connection, std::size_t left2read) {
    if (!error) {
      ::fwrite(boost::begin(range), size, 1, fp);
      std::size_t left = left2read - size;
      if (left > 0)
        read_chunk(connection, left);
      else
        wakeup();
    }
  }

  ///
  /// Wakesup the waiting thread
  ///
  void wakeup() {
    std::unique_lock<std::mutex> _(mtx);
    condvar.notify_one();
  }
};

struct file_server {
  explicit file_server(file_cache &cache, void *pUserContext)
      : cache_(cache), pUserContext(pUserContext) {}

  void operator()(server::request const &request,
                  server::connection_ptr connection) {
    // std::cout << "Destination: " << request.destination << std::endl;
    // std::cout << "Method: " << request.method << std::endl;

    if (request.method == "HEAD") {
      std::shared_ptr<connection_handler> h(new connection_handler(cache_));
      (*h)(request.destination, connection, false);
    } else if (request.method == "GET") {
      std::shared_ptr<connection_handler> h(new connection_handler(cache_));
      (*h)(request.destination, connection, true);
    } else if (request.method == "PUT" || request.method == "POST") {
      const std::string dest = destination(request);

      if (dest.find("/upload") != std::string::npos) {
        std::map<std::string, std::string> headers = {
            {"Connection", "close"},
            {"Content-Length", "0"},
            {"Content-Type", "text/plain"}};

        try {
          auto start = std::chrono::high_resolution_clock::now();
          // Create a file uploader
          std::shared_ptr<file_uploader> uploader(
              new file_uploader(request, connection));
          // On success to create, start receiving the data
          uploader->async_recv();
          // Wait until the data transfer is done by the IO threads
          uploader->wait_for_completion();

          auto end = std::chrono::high_resolution_clock::now();
          std::chrono::duration<double, std::milli> diff = end - start;
          std::ostringstream stm;
          stm << "Took " << diff.count() << " milliseconds for the transfer."
              << std::endl;
          auto body = stm.str();
          // Respond to the client
          headers["Content-Length"] = std::to_string(body.size());
          connection->set_status(server::connection::ok);
          connection->set_headers(headers);
          connection->write(body);
        } catch (const file_uploader_exception &e) {
          const std::string err = e.what();
          headers["Content-Length"] = std::to_string(err.size());
          connection->set_status(server::connection::bad_request);
          connection->set_headers(headers);
          connection->write(err);
        }
      } else {
        auto h = std::make_shared<connection_handler>(cache_);
        auto c = std::make_shared<input_consumer>(h, request, pUserContext);
        connection->read([c](
            server::connection::input_range input, boost::system::error_code ec,
            std::size_t bytes_transferred, server::connection_ptr connection) {
          (*c)(input, ec, bytes_transferred, connection);
        });
      }
    } else {
      static server::response_header error_headers[] = {
          {"Connection", "close"}};
      connection->set_status(server::connection::not_supported);
      connection->set_headers(
          boost::make_iterator_range(error_headers, error_headers + 1));
      connection->write("Method not supported.");
      std::cout << "Method not supported." << std::endl;
    }
  }

  file_cache &cache_;
  void *pUserContext;
};

FileServer::FileServer(void *pUserContext) {
  try {
    file_cache cache(".");
    file_server handler(cache, pUserContext);
    server::options options(handler);
    server instance(options.thread_pool(std::make_shared<utils::thread_pool>(4))
                        .address("0.0.0.0")
                        .port("8080")
                        .reuse_address(true));
    instance.run();
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;

    DCT_ASSERT(e.what());
  }
}
