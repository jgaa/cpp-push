
#include <chrono>
#include <span>
#include <boost/json.hpp>
#include <boost/url.hpp>
#include "cpp-push/ApplePusher.h"
#include "cpp-push/logging.h"

#include <jwt-cpp/jwt.h>

namespace json = boost::json;
using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std;

namespace jgaa::cpp_push {

ApplePusher::ApplePusher(const Config &cfg, boost::asio::io_context &ctx)
    : Pusher(cfg), ctx_(ctx)
{

}

ApplePusher::~ApplePusher()
{

}

void ApplePusher::run()
{
    boost::asio::co_spawn(ctx_, std::bind(&ApplePusher::run_, this),
                          boost::asio::detached);
}

boost::asio::awaitable<void> ApplePusher::run_()
{
    LOG_INFO_N << "Starting...";

    while(state() <= State::ERROR) {
        try {
            createJwtToken();
            const auto token = getAuth();
            const int refresh_after = chrono::duration_cast<chrono::minutes>(
                                          token->expiry - std::chrono::system_clock::now()
                                          - std::chrono::minutes(config().apple.jwt_refresh_minutes))
                                          .count();
            LOG_TRACE_N << "Token expires in "
                        << chrono::duration_cast<chrono::minutes>(token->expiry - std::chrono::system_clock::now()).count()
                        << " minutes, refreshing after " << refresh_after << " minutes";
            jwt_timer_.expires_after(chrono::minutes(max(1, refresh_after)));
            setState(State::AVAILABLE);
        } catch (const std::exception& e) {
            LOG_WARN_N << "Error creating JWT token: " << e.what();
            setState(State::ERROR);
            jwt_timer_.expires_after(chrono::seconds(30));
        }

        try {
            co_await jwt_timer_.async_wait(boost::asio::use_awaitable);
        } catch (const boost::system::system_error& e) {
            if (e.code() != boost::asio::error::operation_aborted) {
                LOG_WARN_N << "JWT timer error: " << e.what();
                setState(State::ERROR);
            }
        }
    }

    rest().Close();
    setState(State::STOPPED);
    LOG_INFO_N << "Done.";
}

void ApplePusher::createJwtToken()
{
    using namespace std::chrono;
    auto now = system_clock::now();
    auto exp = now + minutes(config().apple.jwt_ttl_minutes);

    const auto& c = config().apple;

    auto p8 = std::ifstream(c.p8_key_path);

    if (!p8.is_open()) {
        throw std::runtime_error(
            "Unable to open APNs key file: " + c.p8_key_path.string()
            );
    }
    std::string private_key{
        std::istreambuf_iterator<char>(p8),
        {}
    };
    if (private_key.empty()) {
        throw std::runtime_error(
            "APNs key file is empty: " + c.p8_key_path.string()
            );
    }
    auto jwt = jwt::create()
                     .set_issuer(c.team_id)
                     .set_issued_at(now)
                     .set_expires_at(exp)
                     .set_type("JWT")
                     .set_key_id(c.key_id)
                     .sign(jwt::algorithm::es256("", private_key));

    auto token = make_shared<OAuthToken>(
        std::move(jwt),
        exp
        );

    setAuthToken(std::move(token));
}

void ApplePusher::stop()
{
    LOG_INFO_N << "Stopping ApplePusher...";
    setState(State::STOPPING);
    jwt_timer_.cancel();
}

// ApplePush.cpp

boost::asio::awaitable<Pusher::Result>
ApplePusher::apush(const ApplePushMessage& pm)
{
    static constexpr array<string_view, 2> hosts = {
        "https://api.sandbox.push.apple.com", "https://api.push.apple.com"
    };

    const auto host = config().apple.use_sandbox ? hosts[0] : hosts[1];

    const auto auth_token = getAuth();
    const auto bearer = std::format("Bearer {}", auth_token->access_token);
    unsigned successes = 0;

    for (auto const& token : PushMessage::tokens_view{pm.to}) {
        // --- build JSON payload ---
        boost::json::object aps;
        if (pm.notification) {
            boost::json::object alert;
            if (!pm.notification->title.empty())
                alert["title"] = pm.notification->title;
            if (!pm.notification->body.empty())
                alert["body"]  = pm.notification->body;
            aps["alert"] = alert;

            if (!pm.notification->sound.empty())
                aps["sound"] = pm.notification->sound;
            if (pm.notification->badge)
                aps["badge"] = *pm.notification->badge;
            if (pm.notification->thread_id)
                aps["thread-id"] = *pm.notification->thread_id;
        }
        // root = { "aps": { … }, customKey: customValue, … }
        boost::json::object root;
        root["aps"] = std::move(aps);
        for (auto& kv : pm.data) {
            root[std::string(kv.first)] = std::string(kv.second);
        }
        auto body = boost::json::serialize(root);

        try {
            const auto exp_ts = std::chrono::duration_cast<std::chrono::seconds>(
                                    pm.expiration.time_since_epoch()).count();
            const auto url = format("{}/3/device/{}", host, token);
            const auto res = co_await rest().Build()->Post(url)
                .Header("Authorization", bearer)
                .Option( CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS)
                .Header("apns-topic", config().apple.topic)
                .Header("apns-push-type",
                    pm.type == PushMessage::PushType::DATA ? "background" : "alert")
                .Header("apns-priority",  std::to_string(pm.priority))
                .Header("apns-expiration", std::to_string(exp_ts))
                .WithJson()
                .AcceptJson()
                .SendData(body)
                .AsioAsyncExecute(boost::asio::use_awaitable);

            if (!res.isOk()) {
                LOG_WARN_N << "Failed to send push message: "
                           << res.msg;
                co_return Pusher::Result{false, res.msg, successes};
            }

            ++successes;

        } catch (const boost::system::system_error& e) {
            LOG_WARN_N << "Failed to send push message: " << e.what();
            co_return Pusher::Result{false, e.code().message(), successes};
        }
        catch (const exception& e) {
            LOG_WARN_N << "Failed to send push message: " << e.what();
            co_return Pusher::Result{false, e.what(), successes};
        }
    }

    co_return Pusher::Result{successes};
}

boost::asio::awaitable<Pusher::Result>
ApplePusher::push(const PushMessage& pm)
{
    ApplePushMessage apm;
    apm.to         = pm.to;
    apm.data       = pm.data;
    apm.type       = pm.type;

    // set expiry N minutes in the future
    apm.expiration = std::chrono::system_clock::now()
                     + std::chrono::minutes{ config().apple.jwt_ttl_minutes };

    // priority: background for DATA, immediate for NOTIFICATION
    apm.priority   = (pm.type == PushMessage::PushType::DATA ? 5 : 10);

    if (pm.notification) {
        AppleNotification an;
        an.title  = pm.notification->title;
        an.body   = pm.notification->body;
        an.sound  = pm.notification->sound;
        // optionally set badge/thread-id if you want defaults
        apm.notification = std::move(an);
    }

    co_return co_await apush(apm);
}

std::shared_ptr<Pusher> createPusherForApple(const Config& config, boost::asio::io_context& ctx) {
    auto p = std::make_shared<ApplePusher>(config, ctx);
    p->run();
    return p;
}

} // ns
