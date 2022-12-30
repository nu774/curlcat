#include <memory>
#include <curl/curl.h>
#include <curl/easy.h>
#include <curl/multi.h>

namespace curl {
  struct global_scope {
    global_scope() {
      curl_global_init(CURL_GLOBAL_ALL);
    }
    ~global_scope() {
      curl_global_cleanup();
    }
  };

  class slist {
    curl_slist *list_;
  public:
    slist(): list_(nullptr) {}
    ~slist() {
      curl_slist_free_all(list_);
    }
    slist(const slist&) = delete;
    slist& operator=(const slist&) = delete;

    curl_slist *get() { return list_; }

    void append(const char *value) {
      list_ = curl_slist_append(list_, value);
    }
  };

  class easy_handle {
    std::shared_ptr<CURL> handle_;
  public:
    easy_handle(): handle_(curl_easy_init(), curl_easy_cleanup) {}

    CURL *get() { return handle_.get(); }

    template <typename PARAM>
    CURLcode setopt(CURLoption opt, PARAM&& parameter) {
      return curl_easy_setopt(get(), opt, parameter);
    }

    CURLcode pause(int bitmask) {
      return curl_easy_pause(get(), bitmask);
    }
  };

  class multi_handle {
    std::shared_ptr<CURLM> handle_;
  public:
    multi_handle(): handle_(curl_multi_init(), curl_multi_cleanup) {}

    CURLM *get() { return handle_.get(); }

    CURLMcode add_handle(CURL *handle) {
      return curl_multi_add_handle(get(), handle);
    }
    CURLMcode remove_handle(CURL *handle) {
      return curl_multi_remove_handle(get(), handle);
    }
    CURLMcode poll(curl_waitfd *extra_fds, unsigned extra_nfds, int timeout_ms, int *numfds) {
      return curl_multi_poll(get(), extra_fds, extra_nfds, timeout_ms, numfds);
    }
    CURLMcode perform(int *running_handles) {
      return curl_multi_perform(get(), running_handles);
    }
    CURLMsg *info_read(int *msgs_in_queue) {
      return curl_multi_info_read(get(), msgs_in_queue);
    }
  };
}
