#include <fstream>
#include <filesystem>
#include <iostream>
#include <memory>
#include <atomic>
#include <list>

#include <boost/program_options.hpp>

#include "pulsartests.h"

using namespace std;

bool WaitForPulsar(pulsar::Client& client, const Config& conf) {
    ostringstream topic;
    topic << "persistent:/"
        << '/' << conf.tenant_name
        << '/' << conf.namespace_name
        << '/' << "probe";

    auto topic_str = topic.str();

    for(int i = 0; i < 60; ++i) {
        pulsar::Consumer consumer;
        const auto res = client.subscribe(topic_str, "me", consumer);
        if (res != pulsar::ResultOk) {
            this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        consumer.close();
        return true;
    }

    return false;
}

int main(int argc, char *argv[])
{
    namespace po = boost::program_options;
    namespace logging = boost::log;

    Config config;
    string log_level;

    po::options_description general("General Options");

    general.add_options()
        ("help,h", "Print help and exit")
        ("version", "Print version and exit")
        ("log-level,l", po::value<string>(&log_level)->default_value("info"),
            "Log-level for the log-file")
        ;

    po::options_description testopts("Test Options");
    testopts.add_options()
        ("pulsar-url", po::value<string>(&config.pulsar_url)->default_value(config.pulsar_url),
            "Pulsar service url.")
        ("pulsar-threads", po::value<size_t>(&config.pulsar_client_threads)->default_value(config.pulsar_client_threads),
            "Number of IO threads used by the Pulsar client.")
        ("asio-threads", po::value<size_t>(&config.asio_threads)->default_value(config.asio_threads),
            "Number of threads used by asio for the producer.")
        ("messages", po::value<size_t>(&config.messages)->default_value(config.messages),
            "Number of messages to send on each topic. 0 for unlimited.")
        ("duration", po::value<size_t>(&config.test_duration)->default_value(config.test_duration),
            "Number of seconds to produce messages before the producers stop. 0 for unlimited.")
        ("message-size", po::value<size_t>(&config.message_size)->default_value(config.message_size),
            "Size of each message.")
        ("messages-per-second", po::value<size_t>(&config.produce_messages_per_second)->default_value(config.produce_messages_per_second),
            "How many messages to produce per second for each topic.")
        ("namespaces", po::value<size_t>(&config.namespaces)->default_value(config.namespaces),
            "Number of namespaces.")
        ("topics", po::value<size_t>(&config.topics)->default_value(config.topics),
            "Number of topics in each namespace.")
        ("producer", po::value<bool>(&config.producer)->default_value(config.producer)->default_value(config.producer),
            "Produser switch.")
        ("consumer", po::value<bool>(&config.consumer)->default_value(config.consumer)->default_value(config.consumer),
            "Consumer switch.")
        ("producer-batching", po::value<bool>(&config.producer_batching)->default_value(config.producer_batching)->default_value(config.producer_batching),
            "Consumer batching switch.")
        ("stats_interval", po::value<int>(&config.stats_interval)->default_value(config.stats_interval),
            "Interval (in seconds) between stats dumps in the log. 0 to disable.")
        ("report-file", po::value<string>(&config.report_file)->default_value(config.report_file),
            "CSV file to write resultys to.")
        ("storage", po::value<string>(&config.storage)->default_value(config.storage),
            "Storage used (info).")
        ("where", po::value<string>(&config.location)->default_value(config.location),
            "Where the tests were ran (info).")
        ("pulsar-deployment-cpus", po::value<string>(&config.pulsar_deployment_cpus)->default_value(config.pulsar_deployment_cpus),
            "How many CPU's available to Pulsar during the test (info).")
        ("pulsar-deployment-ram", po::value<string>(&config.pulsar_deployment_ram)->default_value(config.pulsar_deployment_ram),
            "How much RAM was available to Pulsar during the test (info).")
        ;

    po::options_description cmdline_options;
    cmdline_options.add(general).add(testopts);

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, cmdline_options), vm);
    po::notify(vm);

    if (vm.count("help")) {
        cout << cmdline_options << endl
            << "Log-levels are:" << endl
            << "   trace debug info " << endl;

        return -2;
    }

    if (vm.count("version")) {
        cout << "pulsartest " << PROJECT_VERSION << endl;
        return -2;
    }


    auto llevel = logging::trivial::info;
    if (log_level == "debug") {
        llevel = logging::trivial::debug;
    } else if (log_level == "trace") {
        llevel = logging::trivial::trace;
    } else if (log_level == "info") {
        ; // Do nothing
    } else {
        std::cerr << "Unknown log-level: " << log_level << endl;
        return  -1;
    }

    logging::core::get()->set_filter
    (
        logging::trivial::severity >= llevel
    );

    if (!config.producer && !config.consumer) {
        BOOST_LOG_TRIVIAL( error) << "Nothing to do. I'm neither a producer nor a cosnumer!";
        return -3;
    }

    try {
        pulsar::ClientConfiguration cc;
        cc.setIOThreads(config.pulsar_client_threads);
        cc.setUseTls(false);
        cc.setStatsIntervalInSeconds(config.stats_interval);
        cc.setLogger({});
        pulsar::Client pclient{config.pulsar_url, cc};

        // Wait for pulsar to come up (as needed)
        if (!WaitForPulsar(pclient, config)) {
            BOOST_LOG_TRIVIAL ( error ) << "Cannot subscribe to Pulsar.";
            return -1;
        }

        // TODO: Setup namespaces and topics
        // For now, just use the provided resources in the standalone container.

        Base::ptr_t consumer;
        Base::ptr_t producer;
        std::list<std::future<void>> futures;
        boost::asio::io_context ctx;
        std::list<std::thread> workers;

        BOOST_LOG_TRIVIAL( info ) << "Getting myself ready...";
        boost::asio::io_service::work asio_work{ctx};

        for(size_t t = 0; t < config.asio_threads; ++t) {
            thread thd([t, &ctx]() {
                BOOST_LOG_TRIVIAL ( debug ) << "Starting asio loop " << t;
                auto name = "asio-"s + to_string(t);
                SetThreadName(name.c_str());
                ctx.run();
                BOOST_LOG_TRIVIAL ( debug ) << "Done with asio loop " << t;
            });

            workers.push_back(move(thd));
        }

        SetThreadName("main");
        Timer main_timer;

        if (config.consumer) {
            consumer = Base::CreateConsumer(ctx, pclient, config);
            futures.push_back(consumer->Run());
        }

        if (config.producer) {
            producer = Base::CreateProducer(ctx, pclient, config);
            futures.push_back(producer->Run());
        }

        // Wait patiently for the tests to complete...
        for(auto& f: futures) {
            f.get();
        }

        pclient.shutdown();
        ctx.stop();

        for(auto& w : workers) {
            w.join();
        }

        const auto elapsed = main_timer.elapsedSeconds();

        if (!filesystem::exists(config.report_file)) {
            std::ofstream hdr{config.report_file};
            hdr << "mps, messages, streams, pulsar-threads, asio-threads, batching, s-duration, s-ok, s-failed, s-avg, s-total-avg, "
                << "r-duration, r-ok, r-failed, f-failed-ack, r-avg, r-total-avg, app-time, "
                << "storage, where, pulsar-cpus, pulsar-ram"
                << endl;
        }
        {
            std::ofstream csv{config.report_file, ofstream::app};
            csv << fixed
                << config.produce_messages_per_second << ','
                << config.messages << ','
                << config.topics << ','
                << config.pulsar_client_threads  << ','
                << config.asio_threads << ','
                << config.producer_batching << ','

                << producer->GetResults().duration_ << ','
                << producer->GetResults().ok_messages << ','
                << producer->GetResults().failed_messages << ','
                << producer->GetResults().avg_per_sec << ','
                << producer->GetResults().aggregated_avg_per_sec << ','

                << consumer->GetResults().duration_ << ','
                << consumer->GetResults().ok_messages << ','
                << consumer->GetResults().failed_messages << ','
                << consumer->GetResults().failed_ack << ','
                << consumer->GetResults().avg_per_sec << ','
                << consumer->GetResults().aggregated_avg_per_sec << ','
                << elapsed << ','

                << config.storage << ','
                << config.location << ','
                << config.pulsar_deployment_cpus << ','
                << config.pulsar_deployment_ram

                << endl;
        }

        BOOST_LOG_TRIVIAL( info ) << "Done after " << elapsed << " seconds";

    } catch (const std::exception& ex) {
        BOOST_LOG_TRIVIAL( error) << "Caught exacption in main: " << ex.what();
        return -5;
    }

    return 0;
}
