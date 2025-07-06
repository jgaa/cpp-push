
#include <atomic>

#include "cpp-push/Pusher.h"


namespace jgaa::cpp_push {

class GooglePusher : public Pusher {
public:
    struct ServiceAccount {
        std::string type;
        std::string project_id;
        std::string private_key_id;
        std::string private_key;
        std::string client_email;
        std::string client_id;
        std::string auth_uri;
    };

    /*! Constructor initializing the GooglePusher with the given configuration.
     * @param config The configuration for the GooglePusher.
     */
    explicit GooglePusher(const Config& config, boost::asio::io_context& ctx);

    /*! Push a message to the remote server.
     * @param message The message to be pushed.
     * @return Result indicating success or failure of the push operation.
     */
    [[nodiscard]] virtual boost::asio::awaitable<Result> push(const std::string& data) override;

    virtual bool isReady() const noexcept override {
        return available_.load(std::memory_order_relaxed);
    }

    void run();
    void stop() override;

private:
    boost::asio::awaitable<void> run_();
    boost::asio::awaitable<std::string> createJwtToken() const;
    void startNextJwtTimer();
    void loadServiceAccount();

    Config config_;
    boost::asio::io_context& ctx_;
    boost::asio::deadline_timer jwt_timer_{ctx_};
    ServiceAccount service_account_;
    std::atomic_bool available_{false}; // Indicates if the pusher is available
    std::mutex mutex_;
};

}
