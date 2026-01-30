/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2012 Tatsuhiro Tsujikawa
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "nghttp2_config.h"

#ifdef __sgi
#  define daemon _daemonize
#endif

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif // HAVE_UNISTD_H
#include <signal.h>
#include <getopt.h>

#include <cstdlib>
#include <cstring>
#include <cassert>
#include <string>
#include <iostream>
#include <string>

#include <nghttp2/nghttp2.h>

#include "app_helper.h"
#include "HttpServer.h"
#include "util.h"
#include "tls.h"

namespace nghttp2 {

namespace {
int parse_push_config(Config &config, const char *optarg) {
  const char *eq = strchr(optarg, '=');
  if (eq == nullptr) {
    return -1;
  }
  auto &paths = config.push[std::string(optarg, eq)];
  auto optarg_end = optarg + strlen(optarg);
  auto i = eq + 1;
  for (;;) {
    const char *j = strchr(i, ',');
    if (j == nullptr) {
      j = optarg_end;
    }
    paths.emplace_back(i, j);
    if (j == optarg_end) {
      break;
    }
    i = j;
    ++i;
  }

  return 0;
}
} // namespace

namespace {
void print_version(std::ostream &out) {
  out << "nghttpd nghttp2/" NGHTTP2_VERSION << std::endl;
}
} // namespace

namespace {
void print_usage(std::ostream &out) {
  out << "Usage: nghttpd [OPTION]... <PORT> [<PRIVATE_KEY> <CERT>]\n"
      << "HTTP/2 server" << std::endl;
}
} // namespace

namespace {
void print_help(std::ostream &out) {
  Config config;
  print_usage(out);
  out << R"(
  <PORT>      Specify listening port number.
  <PRIVATE_KEY>
              Set  path  to  server's private  key.   Required  unless
              --no-tls is specified.
  <CERT>      Set  path  to  server's  certificate.   Required  unless
              --no-tls is specified.
Options:
  -a, --address=<ADDR>
              The address to bind to.  If not specified the default IP
              address determined by getaddrinfo is used.
  -D, --daemon
              Run in a background.  If -D is used, the current working
              directory is  changed to '/'.  Therefore  if this option
              is used, -d option must be specified.
  -V, --verify-client
              The server  sends a client certificate  request.  If the
              client did  not return  a certificate, the  handshake is
              terminated.   Currently,  this  option just  requests  a
              client certificate and does not verify it.
  -d, --htdocs=<PATH>
              Specify document root.  If this option is not specified,
              the document root is the current working directory.
  -v, --verbose
              Print debug information  such as reception/ transmission
              of frames and name/value pairs.
  --no-tls    Disable SSL/TLS.
  -c, --header-table-size=<SIZE>
              Specify decoder header table size.
  --encoder-header-table-size=<SIZE>
              Specify encoder header table size.  The decoder (client)
              specifies  the maximum  dynamic table  size it  accepts.
              Then the negotiated dynamic table size is the minimum of
              this option value and the value which client specified.
  --color     Force colored log output.
  -p, --push=<PATH>=<PUSH_PATH,...>
              Push  resources <PUSH_PATH>s  when <PATH>  is requested.
              This option  can be used repeatedly  to specify multiple
              push  configurations.    <PATH>  and   <PUSH_PATH>s  are
              relative  to   document  root.   See   --htdocs  option.
              Example: -p/=/foo.png -p/doc=/bar.css
  -b, --padding=<N>
              Add at  most <N>  bytes to a  frame payload  as padding.
              Specify 0 to disable padding.
  -m, --max-concurrent-streams=<N>
              Set the maximum number of  the concurrent streams in one
              HTTP/2 session.
              Default: )"
      << config.max_concurrent_streams << R"(
  -n, --workers=<N>
              Set the number of worker threads.
              Default: 1
  -e, --error-gzip
              Make error response gzipped.
  -w, --window-bits=<N>
              Sets the stream level initial window size to 2**<N>-1.
  -W, --connection-window-bits=<N>
              Sets  the  connection  level   initial  window  size  to
              2**<N>-1.
  --dh-param-file=<PATH>
              Path to file that contains  DH parameters in PEM format.
              Without  this   option,  DHE   cipher  suites   are  not
              available.
  --early-response
              Start sending response when request HEADERS is received,
              rather than complete request is received.
  --trailer=<HEADER>
              Add a trailer  header to a response.   <HEADER> must not
              include pseudo header field  (header field name starting
              with ':').  The  trailer is sent only if  a response has
              body part.  Example: --trailer 'foo: bar'.
  --hexdump   Display the  incoming traffic in  hexadecimal (Canonical
              hex+ASCII display).  If SSL/TLS  is used, decrypted data
              are used.
  --echo-upload
              Send back uploaded content if method is POST or PUT.
  --mime-types-file=<PATH>
              Path  to file  that contains  MIME media  types and  the
              extensions that represent them.
              Default: )"
      << config.mime_types_file << R"(
  --no-content-length
              Don't send content-length header field.
  --ktls      Enable ktls.
  --version   Display version information and exit.
  -h, --help  Display this help and exit.

--

  The <SIZE> argument is an integer and an optional unit (e.g., 10K is
  10 * 1024).  Units are K, M and G (powers of 1024).)"
      << std::endl;
}
} // namespace

int main(int argc, char **argv) {
  Config config;
  bool color = false;
  auto mime_types_file_set_manually = false;

  while (1) {
    static int flag = 0;
    constexpr static option long_options[] = {
      {"address", required_argument, nullptr, 'a'},
      {"daemon", no_argument, nullptr, 'D'},
      {"htdocs", required_argument, nullptr, 'd'},
      {"help", no_argument, nullptr, 'h'},
      {"verbose", no_argument, nullptr, 'v'},
      {"verify-client", no_argument, nullptr, 'V'},
      {"header-table-size", required_argument, nullptr, 'c'},
      {"push", required_argument, nullptr, 'p'},
      {"padding", required_argument, nullptr, 'b'},
      {"max-concurrent-streams", required_argument, nullptr, 'm'},
      {"workers", required_argument, nullptr, 'n'},
      {"error-gzip", no_argument, nullptr, 'e'},
      {"window-bits", required_argument, nullptr, 'w'},
      {"connection-window-bits", required_argument, nullptr, 'W'},
      {"no-tls", no_argument, &flag, 1},
      {"color", no_argument, &flag, 2},
      {"version", no_argument, &flag, 3},
      {"dh-param-file", required_argument, &flag, 4},
      {"early-response", no_argument, &flag, 5},
      {"trailer", required_argument, &flag, 6},
      {"hexdump", no_argument, &flag, 7},
      {"echo-upload", no_argument, &flag, 8},
      {"mime-types-file", required_argument, &flag, 9},
      {"no-content-length", no_argument, &flag, 10},
      {"encoder-header-table-size", required_argument, &flag, 11},
      {"ktls", no_argument, &flag, 12},
      {"no-rfc7540-pri", no_argument, &flag, 13},
      {nullptr, 0, nullptr, 0}};
    int option_index = 0;
    int c = getopt_long(argc, argv, "DVb:c:d:ehm:n:p:va:w:W:", long_options,
                        &option_index);
    if (c == -1) {
      break;
    }
    switch (c) {
    case 'a':
      config.address = optarg;
      break;
    case 'D':
      config.daemon = true;
      break;
    case 'V':
      config.verify_client = true;
      break;
    case 'b': {
      auto n = util::parse_uint(optarg);
      if (!n) {
        std::cerr << "-b: Bad option value: " << optarg << std::endl;
        exit(EXIT_FAILURE);
      }
      config.padding = *n;
      break;
    }
    case 'd':
      config.htdocs = optarg;
      break;
    case 'e':
      config.error_gzip = true;
      break;
    case 'm': {
      // max-concurrent-streams option
      auto n = util::parse_uint(optarg);
      if (!n) {
        std::cerr << "-m: invalid argument: " << optarg << std::endl;
        exit(EXIT_FAILURE);
      }
      config.max_concurrent_streams = *n;
      break;
    }
    case 'n': {
#ifdef NOTHREADS
      std::cerr << "-n: WARNING: Threading disabled at build time, "
                << "no threads created." << std::endl;
#else
      auto n = util::parse_uint(optarg);
      if (!n) {
        std::cerr << "-n: Bad option value: " << optarg << std::endl;
        exit(EXIT_FAILURE);
      }
      config.num_worker = *n;
#endif // NOTHREADS
      break;
    }
    case 'h':
      print_help(std::cout);
      exit(EXIT_SUCCESS);
    case 'v':
      config.verbose = true;
      break;
    case 'c': {
      auto n = util::parse_uint_with_unit(optarg);
      if (!n) {
        std::cerr << "-c: Bad option value: " << optarg << std::endl;
        exit(EXIT_FAILURE);
      }
      if (n > std::numeric_limits<uint32_t>::max()) {
        std::cerr << "-c: Value too large.  It should be less than or equal to "
                  << std::numeric_limits<uint32_t>::max() << std::endl;
        exit(EXIT_FAILURE);
      }
      config.header_table_size = *n;
      break;
    }
    case 'p':
      if (parse_push_config(config, optarg) != 0) {
        std::cerr << "-p: Bad option value: " << optarg << std::endl;
      }
      break;
    case 'w':
    case 'W': {
      auto n = util::parse_uint(optarg);
      if (!n || n > 30) {
        std::cerr << "-" << static_cast<char>(c)
                  << ": specify the integer in the range [0, 30], inclusive"
                  << std::endl;
        exit(EXIT_FAILURE);
      }

      if (c == 'w') {
        config.window_bits = *n;
      } else {
        config.connection_window_bits = *n;
      }

      break;
    }
    case '?':
      util::show_candidates(argv[optind - 1], long_options);
      exit(EXIT_FAILURE);
    case 0:
      switch (flag) {
      case 1:
        // no-tls option
        config.no_tls = true;
        break;
      case 2:
        // color option
        color = true;
        break;
      case 3:
        // version
        print_version(std::cout);
        exit(EXIT_SUCCESS);
      case 4:
        // dh-param-file
        config.dh_param_file = optarg;
        break;
      case 5:
        // early-response
        config.early_response = true;
        break;
      case 6: {
        // trailer option
        auto header = optarg;
        auto value = strchr(optarg, ':');
        if (!value) {
          std::cerr << "--trailer: invalid header: " << optarg << std::endl;
          exit(EXIT_FAILURE);
        }
        *value = 0;
        value++;
        while (isspace(*value)) {
          value++;
        }
        if (*value == 0) {
          // This could also be a valid case for suppressing a header
          // similar to curl
          std::cerr << "--trailer: invalid header - value missing: " << optarg
                    << std::endl;
          exit(EXIT_FAILURE);
        }
        config.trailer.emplace_back(header, value, false);
        util::inp_strlower(config.trailer.back().name);
        break;
      }
      case 7:
        // hexdump option
        config.hexdump = true;
        break;
      case 8:
        // echo-upload option
        config.echo_upload = true;
        break;
      case 9:
        // mime-types-file option
        mime_types_file_set_manually = true;
        config.mime_types_file = optarg;
        break;
      case 10:
        // no-content-length option
        config.no_content_length = true;
        break;
      case 11: {
        // encoder-header-table-size option
        auto n = util::parse_uint_with_unit(optarg);
        if (!n) {
          std::cerr << "--encoder-header-table-size: Bad option value: "
                    << optarg << std::endl;
          exit(EXIT_FAILURE);
        }
        if (n > std::numeric_limits<uint32_t>::max()) {
          std::cerr << "--encoder-header-table-size: Value too large.  It "
                       "should be less than or equal to "
                    << std::numeric_limits<uint32_t>::max() << std::endl;
          exit(EXIT_FAILURE);
        }
        config.encoder_header_table_size = *n;
        break;
      }
      case 12:
        // tls option
        config.ktls = true;
        break;
      case 13:
        // no-rfc7540-pri option
        std::cerr << "[WARNING]: --no-rfc7540-pri option has been deprecated."
                  << std::endl;
        break;
      }
      break;
    default:
      break;
    }
  }
  if (argc - optind < (config.no_tls ? 1 : 3)) {
    print_usage(std::cerr);
    std::cerr << "Too few arguments" << std::endl;
    exit(EXIT_FAILURE);
  }

  {
    auto portStr = argv[optind++];
    auto n = util::parse_uint(portStr);
    if (!n || n > std::numeric_limits<uint16_t>::max()) {
      std::cerr << "<PORT>: Bad value: " << portStr << std::endl;
      exit(EXIT_FAILURE);
    }
    config.port = *n;
  }

  if (!config.no_tls) {
    config.private_key_file = argv[optind++];
    config.cert_file = argv[optind++];
  }

  if (config.daemon) {
    if (config.htdocs.empty()) {
      print_usage(std::cerr);
      std::cerr << "-d option must be specified when -D is used." << std::endl;
      exit(EXIT_FAILURE);
    }
#ifdef __sgi
    if (daemon(0, 0, 0, 0) == -1) {
#else
    if (util::daemonize(0, 0) == -1) {
#endif
      perror("daemon");
      exit(EXIT_FAILURE);
    }
  }
  if (config.htdocs.empty()) {
    config.htdocs = "./";
  }

  if (util::read_mime_types(config.mime_types,
                            config.mime_types_file.c_str()) != 0) {
    if (mime_types_file_set_manually) {
      std::cerr << "--mime-types-file: Could not open mime types file: "
                << config.mime_types_file << std::endl;
    }
  }

  auto &trailer_names = config.trailer_names;
  for (auto &h : config.trailer) {
    trailer_names += h.name;
    trailer_names += ", ";
  }
  if (trailer_names.size() >= 2) {
    trailer_names.resize(trailer_names.size() - 2);
  }

  set_color_output(color || isatty(fileno(stdout)));

  struct sigaction act {};
  act.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &act, nullptr);

  reset_timer();

  HttpServer server(&config);
  if (server.run() != 0) {
    exit(EXIT_FAILURE);
  }
  return 0;
}

} // namespace nghttp2

int main(int argc, char **argv) {
  return nghttp2::run_app(nghttp2::main, argc, argv);
}
