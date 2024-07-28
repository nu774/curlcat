#include <cstdio>
#include <curl/curl.h>
#include <getopt.h>
#include "curl_wrapper.hpp"
#include "util.hpp"

static void usage() {
  std::printf("usage: curlcat [options] URL\n"
  "options:\n"
  "-h, --help              print help message\n"
  "-X, --request <method>  HTTP method\n"
  "-H, --header <header>   HTTP header\n"
  "-k, --insecure          don't verify SSL certificate\n"
  "-x, --proxy <URL>       use this proxy\n"
  "-p, --proxytunnel       use HTTP CONNECT for proxy\n"
  "-v, --verbose           write headers to stderr\n"
  "-u, --unbuffered        disable stdout/stderr buffering\n"
  "--json                  shorthand for -H \"Content-Type: application/json\"\n"
  "--proxy-header <header> HTTP header for proxy\n"
  );
}

static std::size_t read_cb(char *d, std::size_t n, std::size_t l, void *c) {
  auto rb = static_cast<util::read_buffer*>(c);
  auto count = rb->read(d, n * l);
  return count > 0 ? count : rb->eof() ? 0 : CURL_READFUNC_PAUSE;
};

static int debug_cb(CURL *, curl_infotype t, char *d, std::size_t n, void *c) {
  if (t == CURLINFO_HEADER_IN || t == CURLINFO_HEADER_OUT) {
    return std::fwrite(d, 1, n, static_cast<std::FILE*>(c));
  }
  return 0;
};

int main(int argc, char **argv) {
  option long_options[] = {
    { "help", no_argument, 0, 'h' },
    { "request", required_argument, 0, 'X' },
    { "header", required_argument, 0, 'H' },
    { "json", no_argument, 0, 'j' },
    { "insecure", no_argument, 0, 'k' },
    { "proxytunnel", no_argument, 0, 'p' },
    { "proxy", required_argument, 0, 'x' },
    { "unbuffered", no_argument, 0, 'u' },
    { "verbose", no_argument, 0, 'v' },
    { "proxy-header", required_argument, 0, ('p'<<8|'h') },
  };
  curl::global_scope curl_;
  curl::slist headers;
  curl::slist proxy_headers;
  const char *method = "GET";
  bool verify = true;
  bool verbose = false;
  int ch;
  const char *proxy = nullptr;
  bool proxytunnel = false;

  while ((ch = getopt_long(argc, argv, "H:X:x:hjkpuv", long_options, 0)) != EOF) {
    if (ch == 'H')
      headers.append(optarg);
    else if (ch == 'X')
      method = optarg;
    else if (ch == 'h')
      return usage(), 0;
    else if (ch == 'j')
      headers.append("Content-Type: application/json");
    else if (ch == 'k')
      verify = false;
    else if (ch == 'p')
      proxytunnel = true;
    else if (ch == 'x')
      proxy = optarg;
    else if (ch == 'v')
      verbose = true;
    else if (ch == 'u') {
      std::setbuf(stdout, nullptr);
      std::setbuf(stderr, nullptr);
    }
    else if (ch == ('p'<<8|'h'))
      proxy_headers.append(optarg);
    else
      return usage(), 1;
  }
  argc -= optind;
  argv += optind;
  if (argc == 0)
    return usage(), 1;

  util::read_buffer rb(STDIN_FILENO);
  curl::easy_handle easy_handle;
  int upload = 0;
  for (auto &&m: { "PUT", "POST", "PATCH" }) {
    if (std::strcmp(m, method) == 0) {
      upload = 1;
      break;
    }
  }
  easy_handle.setopt(CURLOPT_URL, argv[0]);
  easy_handle.setopt(CURLOPT_CUSTOMREQUEST, method);
  if (headers.get()) {
    easy_handle.setopt(CURLOPT_HTTPHEADER, headers.get());
  }
  easy_handle.setopt(CURLOPT_UPLOAD, upload);
  easy_handle.setopt(CURLOPT_READFUNCTION, read_cb);
  easy_handle.setopt(CURLOPT_READDATA, &rb);
  easy_handle.setopt(CURLOPT_WRITEFUNCTION, std::fwrite);
  easy_handle.setopt(CURLOPT_WRITEDATA, stdout);
  if (!verify) {
    easy_handle.setopt(CURLOPT_SSL_VERIFYHOST, 0L);
    easy_handle.setopt(CURLOPT_SSL_VERIFYPEER, 0L);
  }
  if (verbose) {
    easy_handle.setopt(CURLOPT_VERBOSE, 1L);
    easy_handle.setopt(CURLOPT_DEBUGFUNCTION, debug_cb);
    easy_handle.setopt(CURLOPT_DEBUGDATA, stderr);
  }
  if (proxy)
    easy_handle.setopt(CURLOPT_PROXY, proxy);
  if (proxytunnel)
    easy_handle.setopt(CURLOPT_HTTPPROXYTUNNEL, 1L);
  if (proxy_headers.get())
    easy_handle.setopt(CURLOPT_PROXYHEADER, proxy_headers.get());

  if (std::strcmp(method, "HEAD") == 0) {
    easy_handle.setopt(CURLOPT_NOBODY, 1);
  }

  curl::multi_handle multi_handle;
  multi_handle.add_handle(easy_handle.get());

  curl_waitfd extra_fds[1] = {rb.fd(), CURL_WAIT_POLLIN, 0};
  int still_running;
  do {
    int nfds;
    extra_fds[0].revents = 0;
    CURLMcode mc = multi_handle.perform(&still_running);
    if (mc == CURLM_OK && still_running) {
      unsigned extra_nfds = (upload && rb.has_room()) ? 1 : 0;
      mc = multi_handle.poll(extra_fds, extra_nfds, 0x7fffffff, &nfds);
    }
    if (mc != CURLM_OK) {
      std::fprintf(stderr, "ERROR: %s\n", curl_multi_strerror(mc));
      return 2;
    }
    if (extra_fds[0].revents & CURL_WAIT_POLLIN) {
      rb.fill();
      easy_handle.pause(CURLPAUSE_SEND_CONT);
    }
    CURLMsg *msg;
    int msgs_left;
    while ((msg = multi_handle.info_read(&msgs_left)) != nullptr) {
      if (msg->msg != CURLMSG_DONE) continue;
      if (msg->data.result != CURLE_OK) {
        std::fprintf(stderr, "ERROR: %s\n", curl_easy_strerror(msg->data.result));
      }
      multi_handle.remove_handle(msg->easy_handle);
      break;
    }
  } while (still_running);
  return 0;
}
