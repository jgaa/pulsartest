#pragma once

#if __has_include(<sys/prctl.h>)
#   include <sys/prctl.h>
#   define HAVE_PRCTL
#endif

#ifndef PROJECT_VERSION
#   define PROJECT_VERSION "unknown"
#endif

#include <chrono>
#include <future>
#include <atomic>

#include <pthread.h>

#include <boost/asio.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>

#include <pulsar/Client.h>

struct Config {
    std::string pulsar_url = "pulsar://localhost:6650";
    size_t pulsar_client_threads = 1;
    size_t messages = 0;
    size_t message_size = 1024;
    size_t namespaces = 1;
    size_t topics = 1;
    bool producer = true;
    bool consumer = true;
    std::string cluster_name = "standalone";
    std::string tenant_name = "public";
    std::string namespace_name = "default";
    std::string topic_name = "mytopic";
    bool producer_batching = true;
    size_t produce_messages_per_second = 1000;
    size_t asio_threads = 1;
    size_t producer_buffers = 1024;
    int stats_interval = 0;
    std::string report_file = "pulsartests.csv";
    std::string storage = "";
    std::string location = "";
    std::string pulsar_deployment_cpus = "";
    std::string pulsar_deployment_ram = "";
    size_t test_duration = 60;
};

struct Results {
    uint64_t ok_messages = 0;
    uint64_t failed_messages = 0;
    uint64_t failed_ack = 0; // Receiver end
    double avg_per_sec = 0;
    double aggregated_avg_per_sec = 0;
    double duration_ = 0;
};

class Timer {
public:
  Timer()
    : start_{std::chrono::steady_clock::now()} {}

  // Return elapsed time in nanoseconds
  auto elapsed() const {
      const auto ended = std::chrono::steady_clock::now();
      return std::chrono::duration_cast<std::chrono::nanoseconds>(ended - start_).count();
  }

  double elapsedSeconds() const {
      return elapsed() / 1000000000.0;
  }

  const decltype (std::chrono::steady_clock::now()) start_;
};

inline void SetThreadName(const char *name) {
#ifdef HAVE_PRCTL
    prctl(PR_SET_NAME, reinterpret_cast<unsigned long>(name), 0, 0, 0);
#endif
}

class Base {
public:
    using ptr_t = std::unique_ptr<Base>;

    Base(boost::asio::io_context& ctx, pulsar::Client& client, const Config& conf)
        : ctx_{ctx}, client_{client}, conf_{conf}
    {
    }

    virtual ~Base() = default;

    std::future<void> Run() {
        timer_ = std::make_unique<Timer>();
        Run_();
        return promise_.get_future();
    }

    const Results& GetResults() const noexcept {
        return result_;
    }

    static std::unique_ptr<Base> CreateProducer(boost::asio::io_context& ctx, pulsar::Client& client, const Config& conf);
    static std::unique_ptr<Base> CreateConsumer(boost::asio::io_context& ctx, pulsar::Client& client, const Config& conf);

 protected:
    virtual void Run_() = 0;
    void Shutdown() {
        std::call_once(done_, [&](){
            promise_.set_value();
        });
    }

    boost::asio::io_context& ctx_;
    pulsar::Client& client_;
    const Config& conf_;
    Results result_;
    std::unique_ptr<Timer> timer_;

private:
    std::once_flag done_;
    std::promise<void> promise_;
};
