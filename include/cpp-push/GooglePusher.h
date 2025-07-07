
#include <atomic>
#include <chrono>

#include <restincurl/restincurl.h>

#include "cpp-push/Pusher.h"


namespace jgaa::cpp_push {

class GooglePusher : public Pusher {
public:
    enum class State {
        STARTING,
        AVAILABLE,
        ERROR,
        STOPPING,
        STOPPED
    };

    struct OAuthToken {
        std::string access_token;
        std::chrono::system_clock::time_point expiry;
    };

    struct ServiceAccount {
        std::string type;
        std::string project_id;
        std::string private_key_id;
        std::string private_key;
        std::string client_email;
        std::string client_id;
        std::string auth_uri;
        std::string token_uri;
    };

    struct GoogleNotification : public Notification {
        std::string_view click_action;            // intent action
        std::string_view tag;                     // replace/update existing
        std::string_view color;                   // "#rrggbb"
        std::string_view image_url;               // large image in notification
    };

    enum class AndroidPriority { Normal, High };

    struct GooglePushMessage {
        uint32_t ttl_minutes{60*4}; // Time to live for the message in minutes};
        PushMessage::tokens_t to;    // 1-many devices across platforms
        PushMessage::data_t data;    // always delivered
        PushMessage::PushType type{PushMessage::PushType::DATA}; // default to data push
        AndroidPriority priority{AndroidPriority::Normal}; // Android specific priority
        bool dry_run{false};
        std::optional<GoogleNotification> notification; // Google specific notification
    };

    /*! Constructor initializing the GooglePusher with the given configuration.
     * @param config The configuration for the GooglePusher.
     */
    explicit GooglePusher(const Config& config, boost::asio::io_context& ctx);


    virtual bool isReady() const noexcept override {
        return state_.load(std::memory_order_relaxed) == State::AVAILABLE;
    }

    [[nodiscard]] virtual boost::asio::awaitable<Result> push(const PushMessage& pm) override;
    [[nodiscard]] virtual boost::asio::awaitable<Result> gpush(const GooglePushMessage& pm);

    void run();
    void stop() override;

    std::shared_ptr<OAuthToken> getAuth() const noexcept {
        return auth_token_.load(std::memory_order_relaxed);
    }

private:
    void setState(State state);
    void setAuthToken(std::shared_ptr<OAuthToken> && token) {
        auth_token_.store(std::move(token), std::memory_order_relaxed);
    }

    [[nodiscard]] State getState() const noexcept {
        return state_.load(std::memory_order_relaxed);
    }
    boost::asio::awaitable<void> run_();
    [[nodiscard]] std::string createJwtToken() const;
    [[nodiscard]] boost::asio::awaitable<OAuthToken> getAccessToken();
    void loadServiceAccount();

    Config config_;
    restincurl::Client rest_;
    boost::asio::io_context& ctx_;
    boost::asio::deadline_timer jwt_timer_{ctx_};
    ServiceAccount service_account_;
    std::atomic<State> state_{State::STARTING}; // Indicates if the pusher is available
    std::mutex mutex_;
    std::atomic<std::shared_ptr<OAuthToken>> auth_token_;
};

}
