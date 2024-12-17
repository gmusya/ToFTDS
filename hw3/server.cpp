#include <cpprest/http_listener.h>
#include <cpprest/http_msg.h>
#include <cpprest/json.h>
#include <mutex>
#include <openssl/rsa.h>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "hw3/log.h"

using namespace web;
using namespace web::http;
using namespace web::http::experimental::listener;

#include <iostream>
#include <map>
#include <string>

std::mutex lock;
std::map<std::string, std::string> dictionary;

void handle_get(http_request request) {
  std::lock_guard lg(lock);
  LOG("\nhandle GET\n");

  auto answer = json::value::object();

  for (auto const &p : dictionary) {
    answer[p.first] = json::value::string(p.second);
  }

  request.reply(status_codes::OK, answer);
}

void handle_request(
    http_request request,
    std::function<void(json::value const &, json::value &)> action) {
  auto answer = json::value::object();

  request.extract_json()
      .then([&answer, &action](pplx::task<json::value> task) {
        try {
          auto const &jvalue = task.get();

          if (!jvalue.is_null()) {
            action(jvalue, answer);
          }
        } catch (http_exception const &e) {
          std::cout << e.what() << std::endl;
        }
      })
      .wait();

  request.reply(status_codes::OK, answer);
}

void handle_patch(http_request request) {
  std::lock_guard lg(lock);
  LOG("\nhandle PATCH\n");

  handle_request(request, [](json::value const &jvalue, json::value &answer) {
    if (!jvalue.is_object()) {
      throw http_exception(422, "Object is expected");
    }
    for (auto const &e : jvalue.as_object()) {
      const auto &key = e.first;
      const auto &value = e.second;
      if (!value.is_string()) {
        throw http_exception(422, "Value for " + std::string(key) +
                                      " is not string");
      }
      dictionary[key] = value.as_string();
    }
  });
}

ABSL_FLAG(std::string, host, "0.0.0.0", "");
ABSL_FLAG(uint16_t, port, 0, "");
ABSL_FLAG(uint16_t, min_port, 0, "");
ABSL_FLAG(uint16_t, max_port, 0, "");

int main(int argc, char **argv) {
  absl::ParseCommandLine(argc, argv);

  http_listener listener("http://localhost:10000/");

  listener.support(methods::GET, handle_get);
  listener.support(methods::PATCH, handle_patch);

  try {
    listener.open()
        .then([&listener]() { LOG("\nstarting to listen\n"); })
        .wait();

    while (true) {
      std::string action;
      std::cin >> action;
      if (action == "dump") {
        std::lock_guard lg(lock);
        std::stringstream result;
        result << "{\n";
        for (const auto &[k, v] : dictionary) {
          result << "  " << k << ": " << v << "\n";
        }
        result << "}";
        std::cerr << result.str() << std::endl;
      } else if (action == "clear") {
        std::lock_guard lg(lock);
        dictionary.clear();
      }
    }
  } catch (std::exception const &e) {
    std::cout << e.what() << std::endl;
  }

  return 0;
}