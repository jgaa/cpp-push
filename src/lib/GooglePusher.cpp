
#include <chrono>
#include <span>
#include <boost/json.hpp>
#include <boost/url.hpp>
#include "cpp-push/GooglePusher.h"
#include "cpp-push/logging.h"

#include <jwt-cpp/jwt.h>

namespace json = boost::json;
using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std;

using service_account_t = jgaa::cpp_push::GooglePusher::ServiceAccount;

ostream& operator << (ostream& out, const jgaa::cpp_push::GooglePusher::State& state) {
    constexpr auto states = to_array<string_view>({
                                                   string_view{"STARTING"},
                                                   string_view{"AVAILABLE"},
                                                   string_view{"ERROR"},
                                                   string_view{"STOPPING"},
                                                   string_view{"STOPPED"}
    });

    return out << states.at(static_cast<size_t>(state));
}

namespace boost::json {
service_account_t tag_invoke(json::value_to_tag<service_account_t>, json::value const& jv)
{
    // Throw if root isn’t an object
    json::object const& obj = jv.as_object();

    // Pull out each field with `at()`—throws on missing or wrong type
    return service_account_t{
        value_to<std::string>(obj.at("type")),
        value_to<std::string>(obj.at("project_id")),
        value_to<std::string>(obj.at("private_key_id")),
        value_to<std::string>(obj.at("private_key")),
        value_to<std::string>(obj.at("client_email")),
        value_to<std::string>(obj.at("client_id")),
        value_to<std::string>(obj.at("auth_uri")),
        value_to<std::string>(obj.at("token_uri"))
    };
}
} // ns boost.json

namespace jgaa::cpp_push {

GooglePusher::GooglePusher(const Config &config,  boost::asio::io_context& ctx)
    : config_(config), ctx_{ctx} {

    loadServiceAccount();
}

boost::asio::awaitable<Pusher::Result> GooglePusher::gpush(const GooglePushMessage &pm)
{
    const auto url = format("https://fcm.googleapis.com/v1/projects/{}/messages:send",
                            service_account_.project_id);

    boost::json::object message;

    if (!pm.data.empty()) {
        boost::json::object data;
        for (auto& kv : pm.data)
            data[std::string(kv.first)] = std::string(kv.second);
        message["data"] = data;
    }

    boost::json::object android;
    android["ttl"] = format("{}s", pm.ttl_minutes * 60);
    android["priority"] = (pm.priority == AndroidPriority::High ? "HIGH" : "NORMAL");

    if (pm.notification) {
        boost::json::object notif;
        if (!pm.notification->title.empty()) {
            notif["title"] = pm.notification->title;
        }
        if (!pm.notification->body.empty()) {
            notif["body"] = pm.notification->body;
        }
        if (!pm.notification->sound.empty()) {
            notif["sound"] = pm.notification->sound;
        }
        if (!pm.notification->click_action.empty()) {
            notif["click_action"] = pm.notification->click_action;
        }
        if (!pm.notification->tag.empty()) {
            notif["tag"] = pm.notification->tag;
        }
        if (!pm.notification->color.empty()) {
            notif["color"] = pm.notification->color;
        }
        if (!pm.notification->image_url.empty()) {
            notif["image"] = pm.notification->image_url;
        }
        if (!pm.notification->icon.empty()) {
            notif["icon"] = pm.notification->icon;
        }

        message["notification"] = notif;
    }

    message["android"] = android;
    const auto baerer = format("Bearer {}", getAuth()->access_token);

    auto num_successful = 0u;

    for(const auto& token : PushMessage::tokens_view{pm.to}) {
        boost::json::object root;
        message["token"] = token;
        root["message"] = message;
        // Wrap and optional dry_run
        if (pm.dry_run) {
            root["dry_run"] = true;
        }

        const auto body = boost::json::serialize(root);
        LOG_TRACE_N << "Sending push message to token: " << token.substr(0, 16) << "...";

        try {
            const auto res = co_await rest_.Build()->Post(url)
                .Header("Authorization", baerer)
                .WithJson()
                .AcceptJson()
                .SendData(body)
                .AsioAsyncExecute(boost::asio::use_awaitable);

            if (!res.isOk()) {
                LOG_WARN_N << "Failed to send push message: "
                           << res.msg;
                co_return Pusher::Result{false, res.msg, num_successful};
            }

        } catch (const boost::system::system_error& e) {
            LOG_WARN_N << "Failed to send push message: " << e.what();
            co_return Pusher::Result{false, e.code().message(), num_successful};
        }
    }

    co_return Pusher::Result{num_successful};
}

boost::asio::awaitable<Pusher::Result> GooglePusher::push(const PushMessage &pm)
{
    GooglePushMessage gpm;
    gpm.to = pm.to;
    gpm.data = pm.data;
    gpm.type = pm.type;
    gpm.priority = (pm.type == PushMessage::PushType::DATA) ?
                   GooglePusher::AndroidPriority::High :
                   GooglePusher::AndroidPriority::Normal;
    if (pm.notification) {
        gpm.notification = GoogleNotification{
            pm.notification->title,
            pm.notification->body,
            pm.notification->sound
        };
    }
    co_return co_await gpush(gpm);
}

void GooglePusher::run()
{
    boost::asio::co_spawn(ctx_, std::bind(&GooglePusher::run_, this),
                          boost::asio::detached);
}

void GooglePusher::stop()
{
    LOG_INFO_N << "Stopping GooglePusher...";
    setState(State::STOPPING);
    jwt_timer_.cancel();
}

void GooglePusher::setState(State state)
{
    if (state != state_) {
        LOG_DEBUG_N << "state changed from "
                    << state_ << " to " << state;
        state_ = state;
    }
}

boost::asio::awaitable<void> GooglePusher::run_()
{
    LOG_INFO_N << "Starting...";

    while(state_ <= State::ERROR) {
        try {
            auto token = co_await getAccessToken();
            const int refresh_after = chrono::duration_cast<chrono::minutes>(
                                          token.expiry - std::chrono::system_clock::now()
                                          - std::chrono::minutes(config_.google.jwt_refresh_minutes))
                                          .count();
            LOG_TRACE_N << "Token expires in "
                        << chrono::duration_cast<chrono::minutes>(token.expiry - std::chrono::system_clock::now()).count()
                        << " minutes, refreshing after " << refresh_after << " minutes";
            jwt_timer_.expires_from_now(boost::posix_time::minutes(max(1, refresh_after)));
            setAuthToken(std::make_shared<OAuthToken>(std::move(token)));
            setState(State::AVAILABLE);
        } catch (const std::exception& e) {
            LOG_WARN_N << "Error creating JWT token: " << e.what();
            setState(State::ERROR);
            jwt_timer_.expires_from_now(boost::posix_time::seconds(30));
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

    rest_.Close();
    setState(State::STOPPED);
    LOG_INFO_N << "Done.";
}

std::string GooglePusher::createJwtToken() const
{
    using namespace std::chrono;
    auto now = system_clock::now();
    auto exp = now + minutes(config_.google.jwt_ttl_minutes);

    // PEM‐encoded private key and the key ID from your JSON
    const auto& priv_pem = service_account_.private_key;
    const auto& key_id   = service_account_.private_key_id;

    auto token = jwt::create()
                     .set_issuer(service_account_.client_email)
                     .set_subject(service_account_.client_email)
                     .set_audience(service_account_.token_uri)
                     .set_issued_at(now)
                     .set_expires_at(exp)
                     .set_type("JWT")
                     .set_key_id(key_id)
                     .set_payload_claim("scope",
                                        jwt::claim(std::string("https://www.googleapis.com/auth/firebase.messaging"))
                                        )
                     .sign(jwt::algorithm::rs256{/*pub=*/"", /*priv=*/priv_pem});

    return token;
}


boost::asio::awaitable<GooglePusher::OAuthToken> GooglePusher::getAccessToken()
{
    const auto now = std::chrono::system_clock::now();
    const auto jwt = createJwtToken();

    auto body = format(
        "grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer&assertion={}",
         boost::urls::encode(jwt, boost::urls::unreserved_chars)
        );

    // LOG_TRACE_N << "Requesting access token at " << service_account_.token_uri
    //                << " with body: " << body;

    const auto auth_res = co_await rest_.Build()->Post(service_account_.token_uri)
        .Header("Content-Type", "application/x-www-form-urlencoded")
        .SendData(body)
        .AsioAsyncExecute(boost::asio::use_awaitable);


    if (!auth_res.isOk()) {
        LOG_WARN_N << "Failed to get access token: "
                    << auth_res.msg
                   << ' ' << auth_res.body;
        throw std::runtime_error{"Failed to get access token"};
    }

    boost::system::error_code ec;
    boost::json::value jv = boost::json::parse(auth_res.body, ec);
    if (ec) {
        throw std::runtime_error("Failed to parse JSON: " + ec.message());
    }
    auto& obj = jv.as_object();
    auto it_tok = obj.find("access_token");
    auto it_exp = obj.find("expires_in");
    if (it_tok == obj.end() || it_exp == obj.end()) {
        throw std::runtime_error("Invalid token response: " + auth_res.body);
    }

    OAuthToken token;
    token.access_token = it_tok->value().as_string();
    token.expiry = now + std::chrono::seconds(it_exp->value().as_int64());

    LOG_DEBUG_N << "Got a new OAuth token: "
                << token.access_token.substr(0, 16)
                << "..., expires in "  << std::chrono::duration_cast<std::chrono::minutes>(token.expiry - now).count()
                << " minutes";

    co_return token;
}


void GooglePusher::loadServiceAccount()
{
    std::ifstream in{config_.google.config_file, std::ios::binary};
    if (!in) {
        string_view reason = std::strerror(errno);
        LOG_ERROR_N <<"Failed to open " << config_.google.config_file << ": " << reason;
        throw std::runtime_error{"Failed to open service account file"};
    }

    LOG_DEBUG_N << "Loading service account from " << config_.google.config_file;
    std::string s((std::istreambuf_iterator<char>(in)),
                  std::istreambuf_iterator<char>());

    try {
        json::value jv = json::parse(s);
        service_account_ = json::value_to<service_account_t>(jv);
        LOG_TRACE_N << "Service account loaded successfully.";
    } catch (const std::exception& e) {
        LOG_ERROR_N << "Failed to parse service account JSON: " << e.what();
        throw std::runtime_error{"Failed to parse service account JSON"};
    }

    LOG_INFO_N << "Google service account loaded for project: " << service_account_.project_id;
}

std::shared_ptr<Pusher> createPusherForGoogle(const Config& config, boost::asio::io_context& ctx) {
    auto p = std::make_shared<GooglePusher>(config, ctx);
    p->run();
    return p;
}

} // ns

