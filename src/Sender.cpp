
#include <random>
#include "pulsartests.h"

using namespace std::string_literals;
using namespace std;

class Sender : public Base
{
    struct Generator {
        Generator(Sender& sender, size_t id) : sender_{sender}, strand_{sender_.ctx_}, id_{id} {}

        std::string Name() const {
            return "generator-"s + to_string(id_);
        }

        size_t Id() const noexcept { return id_; }

        void SetProducer(pulsar::Producer && producer) {
            producer_ = move(producer);
        }

        void Start() {
            buffers_.resize(sender_.conf_.producer_buffers);

            // Fill the buffers with random data
            default_random_engine re(sender_.random_device_());
            std::uniform_int_distribution<> dis('A', 'z');
            for(auto& s : buffers_) {
                s.resize(sender_.conf_.message_size);
                for(auto& c: s) {
                    c = dis(re);
                }
            }

            if (sender_.conf_.produce_messages_per_second) {
                // We want the timer to be >= 25 ms to have reliable troughput.
                if (sender_.conf_.produce_messages_per_second <= 40) {
                    ideal_delay_ = 1000 /* ms in one second */ / sender_.conf_.produce_messages_per_second;
                } else {
                    ideal_delay_ = 25; // = 40 times per second
                    messages_per_burst_ = sender_.conf_.produce_messages_per_second / 40;
                }

                BOOST_LOG_TRIVIAL (debug) << Name() << " using " << messages_per_burst_ << " messages per insert burst "
                                          << "and " << ideal_delay_ << " ms delay between bursts.";
            }

            timer_ = make_unique<Timer>();
            ScheduleNext();
        }

        size_t GetNumMessagesSent() const { return num_sent_; }
        size_t GetNumMessagesFailed() const { return num_failed_; }
        double GetAvgMessagePerSecond() const {
            return num_sent_ / timer_->elapsedSeconds();
        }

    private:
        void BurstFeed() {
            strand_.post([this]{
                for(size_t i = 0; i < messages_per_burst_; ++i ) {
                    Feed(i);
                }
                if (!done_) {
                    ScheduleNext();
                }
            });
        }

        void ScheduleNext() {

            if (!ideal_delay_) {
               BurstFeed();
            }

            if (!asio_timer_) {
                asio_timer_ = make_unique<boost::asio::deadline_timer>(sender_.ctx_, boost::posix_time::milliseconds(ideal_delay_));
            } else {
                *asio_timer_ = boost::asio::deadline_timer(sender_.ctx_, boost::posix_time::milliseconds(ideal_delay_));
            }

            asio_timer_->async_wait([this] (const boost::system::error_code& err) {
                if (err) {
                    BOOST_LOG_TRIVIAL( error ) << "Timer failed for " << Name()
                                               << ": " << err.message();
                    //return;
                }

                BurstFeed();
            });
        }

        bool Enough(size_t burstCount) const {
            if (sender_.conf_.messages && (count_ >= sender_.conf_.messages)) {
                return true;
            }

            // No need to check the time more than once in a single burst
            if (!burstCount && sender_.conf_.test_duration && (timer_->elapsedSeconds() >= sender_.conf_.test_duration)) {
                return true;
            }

            return false;
        }

        int ValidateMe() {
            BOOST_LOG_TRIVIAL( trace ) << Name() << " OOOPS!";
            return 1;
        }

        void Feed(size_t burstCount) {

            if (done_) {
                return; // Just ignore the rest of the burst
            }

            const auto current_id = ++count_;
            const bool last = Enough(burstCount);

            const auto duration = chrono::steady_clock::now().time_since_epoch();
            const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
            auto& data = buffers_[count_ % buffers_.size()];
            if (last) {
                data[0] = 0; // Flag end of stream;
            }
            auto msg = pulsar::MessageBuilder()
                    .setSequenceId(current_id)
                    .setEventTimestamp(millis)
                    .setAllocatedContent(data.data(), data.size())
                    .build();

            BOOST_LOG_TRIVIAL( trace ) << Name() << " Sending message " << msg.getMessageId() << ValidateMe();

            // sendAsync is too stupid to just tell us that the queue is full...
            auto queue_full_flag = make_shared<bool>(false);
            producer_.sendAsync(msg, [this, current_id, queue_full_flag](pulsar::Result res, const pulsar::MessageId& messageId) {
                if (res == pulsar::ResultOk) {
                    BOOST_LOG_TRIVIAL( trace ) << Name() << " Ack for message " << messageId << " #" << current_id;
                    ++num_sent_;
                } else {
                    BOOST_LOG_TRIVIAL( debug ) << Name() << " Ack for message #" << current_id
                                                 << " failed: " << res;
                    ++num_failed_;
                    *queue_full_flag = res == pulsar::ResultProducerQueueIsFull;
                }
            });

            if (last && !*queue_full_flag) {
                done_ = true;
                producer_.flushAsync([this](pulsar::Result) {
                    producer_.closeAsync([this](pulsar::Result) {
                        BOOST_LOG_TRIVIAL( trace ) << Name() << " has came to an end.";
                        if (asio_timer_) {
                            asio_timer_->cancel();
                            asio_timer_.reset();
                        }
                        sender_.OnDone(*this);
                    });
                });
            }
        }

        uint64_t ideal_delay_ = 0;
        size_t messages_per_burst_ = 1;
        Sender& sender_;
        boost::asio::io_context::strand strand_;
        pulsar::Producer producer_;
        const size_t id_;
        unique_ptr<Timer> timer_;
        uint64_t count_ = 0;
        std::vector<std::string> buffers_;
        unique_ptr<boost::asio::deadline_timer> asio_timer_;
        size_t num_sent_ = 0;
        size_t num_failed_ = 0;
        bool done_ = false;
    };

public:
    Sender(boost::asio::io_context& ctx, pulsar::Client& client, const Config& conf)
        : Base(ctx, client, conf)
    {
    }

    void OnDone(Generator& generator) {
        const auto id = generator.Id();
        ctx_.post([this] () {
            if (generators_.size() == ++completed_) {

                result_.duration_ = timer_->elapsedSeconds();

                for(const auto& g: generators_) {
                    result_.ok_messages += g->GetNumMessagesSent();
                    result_.failed_messages += g->GetNumMessagesFailed();
                    result_.aggregated_avg_per_sec += g->GetAvgMessagePerSecond();
                }

                result_.avg_per_sec = result_.aggregated_avg_per_sec / generators_.size();

                BOOST_LOG_TRIVIAL (info) << "All producers have finished in "
                                         << result_.duration_ << " sec.";
                BOOST_LOG_TRIVIAL (info) << "Messages sent: " << result_.ok_messages
                                         << ", messages failed: " << result_.failed_messages
                                         << ", avg " << result_.avg_per_sec
                                         << " messages/sec, aggregated: " << result_.aggregated_avg_per_sec << " messages/sec.";

                Shutdown();
            }
        });
    }

protected:
    void Run_() override {
        size_t next_producer = 0;
        for(size_t topic_id = 0; topic_id < conf_.topics; ++topic_id) {
            ostringstream topic;
            topic << "persistent:/"
                //<< '/' << conf_.cluster_name
                << '/' << conf_.tenant_name
                << '/' << conf_.namespace_name
                << '/' << conf_.topic_name << '-' << topic_id;

            auto topic_str = topic.str();
            pulsar::ProducerConfiguration pc;
            pc.setBatchingEnabled(conf_.producer_batching);

            auto generator = make_shared<Generator>(*this, next_producer);

            client_.createProducerAsync(topic_str, pc,
                                   [this, topic_str, generator] (pulsar::Result res, pulsar::Producer producer) {

               if (res != pulsar::ResultOk) {
                   BOOST_LOG_TRIVIAL ( error ) << generator->Name() << ": Failed to create producer for " << topic_str;
                   BOOST_LOG_TRIVIAL ( error ) << "Calling Shutdwon()";
                   Shutdown();
                   return;
               }

               generator->SetProducer(move(producer));
               BOOST_LOG_TRIVIAL ( debug ) << generator->Name()
                                           << " ready to produce to "
                                           << topic_str;

               generator->Start();

               generators_.push_back(generator);
            });

            ++next_producer;
        }
    }

    vector<std::shared_ptr<Generator>> generators_;
    random_device random_device_;
    unique_ptr<boost::asio::io_service::work> asio_work_;
    size_t completed_ = 0;
};

std::unique_ptr<Base> Base::CreateProducer(boost::asio::io_context& ctx, pulsar::Client& client, const Config& conf) {
    return make_unique<Sender>(ctx, client, conf);
}
