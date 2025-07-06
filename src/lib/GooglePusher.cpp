
#include <boost/json.hpp>
#include "cpp-push/GooglePusher.h"
#include "cpp-push/logging.h"

namespace json = boost::json;
using namespace std;

using service_account_t = jgaa::cpp_push::GooglePusher::ServiceAccount;

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
    };
}
} // ns boost.json

namespace jgaa::cpp_push {


GooglePusher::GooglePusher(const Config &config,  boost::asio::io_context& ctx)
    : config_(config), ctx_{ctx} {

    loadServiceAccount();
}

void GooglePusher::run()
{
    boost::asio::co_spawn(ctx_, std::bind(&GooglePusher::run_, this),
                          boost::asio::detached);
}

boost::asio::awaitable<void> GooglePusher::run_()
{
    LOG_INFO_N << "GooglePusher started.";

}

boost::asio::awaitable<std::string> GooglePusher::createJwtToken() const
{

}

void GooglePusher::startNextJwtTimer()
{
    LOG_DEBUG_N << "Starting next JWT timer.";
    jwt_timer_.expires_from_now(boost::posix_time::minutes(config_.google.jwt_refresh_minutes));
    jwt_timer_.async_wait([this](const boost::system::error_code& ec) {
        if (!ec) {
            LOG_DEBUG_N << "JWT timer expired, creating new token.";
            boost::asio::co_spawn(ctx_, createJwtToken(), boost::asio::detached);
        } else {
            LOG_WARN_N << "JWT timer error: " << ec.message();
        }
    });
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
        LOG_DEBUG_N << "Service account loaded successfully.";
    } catch (const std::exception& e) {
        LOG_ERROR_N << "Failed to parse service account JSON: " << e.what();
        throw std::runtime_error{"Failed to parse service account JSON"};
    }

    LOG_INFO_N << "Google service account loaded for project: " << service_account_.project_id;
}

std::shared_ptr<Pusher> createPusherForGoggle(const Config& config, boost::asio::io_context& ctx) {
    auto p = std::make_shared<GooglePusher>(config, ctx);
    p->run();
    return p;
}

} // ns

