
//#include <atomic>
// #include <chrono>

// #include <restincurl/restincurl.h>

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

    [[nodiscard]] virtual boost::asio::awaitable<Result> push(const PushMessage& pm) override;
    [[nodiscard]] virtual boost::asio::awaitable<Result> gpush(const GooglePushMessage& pm);

    void run();
    void stop() override;

private:
    std::shared_ptr<OAuthToken> getAuth() const noexcept {
        return auth_token_.load(std::memory_order_relaxed);
    }

    void setAuthToken(std::shared_ptr<OAuthToken> && token) {
        auth_token_.store(std::move(token), std::memory_order_relaxed);
    }

    void loadServiceAccount();
    [[nodiscard]] std::string createJwtToken() const;
    [[nodiscard]] boost::asio::awaitable<void> run_();
    [[nodiscard]] boost::asio::awaitable<GooglePusher::OAuthToken> getAccessToken();

    ServiceAccount service_account_;
    boost::asio::io_context& ctx_;
    boost::asio::deadline_timer jwt_timer_{ctx_};
    std::atomic<std::shared_ptr<OAuthToken>> auth_token_;
};

}
