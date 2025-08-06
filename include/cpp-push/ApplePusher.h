// ApplePusher.h
#pragma once
#include "Pusher.h"
#include <boost/json.hpp>
#include <jwt-cpp/jwt.h>
#include <chrono>
#include <mutex>

namespace jgaa::cpp_push {

struct AppleNotification : public Notification {
    // iOS-specific overrides or additions (if any)
    // For example, badge number or thread-id
    std::optional<int>   badge;
    std::optional<std::string> thread_id;
};

struct ApplePushMessage {
    using tokens_t = PushMessage::tokens_t;
    tokens_t            to;              // one-to-many device tokens
    PushMessage::data_t data;            // custom payload fields
    PushMessage::PushType type{PushMessage::PushType::DATA};

    // APNs headers
    std::optional<std::string> collapse_id;
    // apns-expiration: seconds since UNIX epoch
    std::chrono::system_clock::time_point expiration;
    // apns-priority: 10 = immediate (alerts), 5 = background
    int priority{10};

    std::optional<AppleNotification> notification;
};


class ApplePusher : public Pusher {
public:
    explicit ApplePusher(const Config& cfg,
                         boost::asio::io_context& ctx);
    ~ApplePusher() override;

    void run();
    void stop() override;

    [[nodiscard]] boost::asio::awaitable<Pusher::Result> apush(const ApplePushMessage &pm);
    [[nodiscard]] boost::asio::awaitable<Pusher::Result> push(const PushMessage &pm) override;

private:
    boost::asio::awaitable<void> run_();
    void createJwtToken();
    std::shared_ptr<OAuthToken> getAuth() const noexcept {
        return auth_token_.load(std::memory_order_relaxed);
    }

    void setAuthToken(std::shared_ptr<OAuthToken> && token) {
        auth_token_.store(std::move(token), std::memory_order_relaxed);
    }
    boost::asio::io_context& ctx_;
    std::atomic<std::shared_ptr<OAuthToken>> auth_token_;
    boost::asio::deadline_timer jwt_timer_{ctx_};
};

} // namespace
