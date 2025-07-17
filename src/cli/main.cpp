
#include <iostream>
#include <optional>

#include <boost/asio.hpp>
#include <boost/program_options.hpp>

#include "cpp-push/cpp-push.h"
#include "cpp-push/logging.h"

using namespace jgaa::cpp_push;
using namespace std;

namespace {

optional<logfault::LogLevel> toLogLevel(string_view name) {
    if (name.empty() || name == "off" || name == "false") {
        return {};
    }

    if (name == "debug") {
        return logfault::LogLevel::DEBUGGING;
    }

    if (name == "trace") {
        return logfault::LogLevel::TRACE;
    }

    return logfault::LogLevel::INFO;
}

}


int main(int argc, char* argv[]) {
    using namespace jgaa::cpp_push;

    // Initialize the configuration
    Config config;
    string log_level_console = "info";

    // Add command-line options using boost::program_options;
    boost::program_options::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "Show help message")
        ("log-to-console,C",
         boost::program_options::value(&log_level_console)->default_value(log_level_console),
         "Log-level to the console; one of 'info', 'debug', 'trace'. Empty string to disable.")
        ("config-file,c", boost::program_options::value<std::filesystem::path>(&config.google.config_file)->required(),
         "Path to the Google service account configuration file")
        ("jwt-ttl", boost::program_options::value<int>(&config.google.jwt_ttl_minutes)->default_value(45),
         "JWT token time to live in minutes")
        ("jwt-refresh", boost::program_options::value<int>(&config.google.jwt_refresh_minutes)->default_value(3),
         "Minutes before expiry to refresh the JWT token");

    //  Add command-line options to allow sending a message. Allow the user to set the values in pm.
    PushMessage pm;
    boost::program_options::options_description msg("Message");
    vector<string> data;
    vector<string> to;
    string message_type= "DATA"; // Default to data message
    string title, body, sound, icon;
    msg.add_options()
        ("to,t", boost::program_options::value(&to),
         "Target device token(s) for the push message. Falls back to envvar PUSH_TOKEN if not provided. "
         "Multiple tokens can be specified by repeating the --to option")
        ("data,d", boost::program_options::value(&data),
         "Data to send in the push message. Each entry contains --data 'name=value' pairs.")
        ("type,y", boost::program_options::value(&message_type)->default_value(message_type),
         "Type of push message (DATA or NOTIFICATION)")
        ("title,T", boost::program_options::value(&title),
         "Title of the notification")
        ("body,B", boost::program_options::value(&body),
         "Body of the notification")
        ("sound,S", boost::program_options::value(&sound),
         "Sound to play for the notification")
        ("icon,I", boost::program_options::value(&icon)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    ,
         "Icon for the notification");

    // Combine the descriptions
    boost::program_options::options_description all_options;
    all_options.add(desc).add(msg);
    // Parse the command-line. Handle exceptions
    boost::program_options::variables_map vm;
    try {
        boost::program_options::store(boost::program_options::parse_command_line(argc, argv, all_options), vm);
        boost::program_options::notify(vm);
    } catch (const boost::program_options::error& e) {
        cerr << "Error parsing command-line options: " << e.what() << std::endl;
        cerr << all_options << std::endl;
        return 1;
    }

    const auto appname = filesystem::path(argv[0]).stem().string();

    // Handle help
    if (vm.count("help")) {
        cout << appname << " [options]";
        cout << all_options << endl;
        return 0;
    }

    if (message_type == "NOTIFICATION") {
        pm.type = PushMessage::PushType::NOTIFICATION;
    } else if (message_type != "DATA") {
        cerr << "Invalid message type: " << message_type << ". Expected 'DATA' or 'NOTIFICATION'." << endl;
        return 1;
    }

    if (!title.empty()|| !body.empty() || !sound.empty() || !icon.empty()) {
        pm.notification.emplace();
        pm.notification->title = title;
        pm.notification->body = body;
        pm.notification->sound = sound;
        pm.notification->icon = icon;
    } else if (pm.type == PushMessage::PushType::NOTIFICATION) {
        cerr << "Notification type selected but no title, body, sound, or icon provided." << endl;
        return 1;
    }

    vector<string_view> tow;

    if (to.empty()) {
        if (auto token = std::getenv("PUSH_TOKEN")) {
            pm.to = std::string_view{token};
            LOG_DEBUG << "Using PUSH_TOKEN environment variable as target token";
        } else {
            throw std::runtime_error("No target token provided. Use --to or set PUSH_TOKEN environment variable.");
        }
    } else if(to.size() == 1) {
        pm.to = to.front();
    } else {
        // Convert vector<string> to vector<string_view>
        tow.reserve(to.size());
        for (string_view token : to) {
            tow.emplace_back(token);
        }
        pm.to = span{tow}; // Use span for multiple tokens};
    }

    // pm.data is just a span of string_views, so we must give it data buffers that will remain valid
    jgaa::cpp_push::PushMessage::data_values_t data_pairs;

    data_pairs.reserve(data.size());
    for(string_view d : data) {
        // Extract the k/v values a string_views and add them to our data container
        auto pos = d.find('=');
        if (pos != std::string::npos) {
            data_pairs.emplace_back(d.substr(0, pos), d.substr(pos + 1));
        } else {
            throw std::runtime_error("Invalid data format, expected key=value pairs");
        }
    }

    pm.data = data_pairs;

    if (auto level = toLogLevel(log_level_console)) {
        logfault::LogManager::Instance().AddHandler(
            make_unique<logfault::StreamHandler>(clog, *level));
    }

    // Create an io_context for asynchronous operations
    boost::asio::io_context io_context;

    // Create a GooglePusher instance
    auto pusher = createPusherForGoogle(config, io_context);
    assert(pusher);

    auto res = boost::asio::co_spawn(io_context, [&]() -> boost::asio::awaitable<int> {

        // Ensure the pusher is ready before proceeding
        for(auto i = 0; i < 30 && !pusher->isReady(); ++i) {
            LOG_DEBUG << "Waiting for pusher to become ready...";
            co_await boost::asio::steady_timer(io_context, std::chrono::seconds(1)).async_wait(boost::asio::use_awaitable);
        }

        try {
            auto res = co_await pusher->push(pm);
            pusher->stop();
            if (res.ok()) {
                LOG_DEBUG << "Push operation completed Ok.";
                co_return 0;
            }
            LOG_WARN << "Push failed: " << res.message();
            co_return 2;
        } catch (const std::exception& e) {
            LOG_ERROR << "Push operation failed: " << e.what();
            io_context.stop();
            co_return 1; // Indicate failure
        }
        assert(false); // Should never reach here
    }, boost::asio::use_future);

    // Run the io_context to process events
    io_context.run();

    // Handle the result of the push operation
    try {
        auto result = res.get();
        if (result == 0) {
            LOG_INFO << "Push message sent successfully!" << std::endl;
            return 0;
        }

        LOG_ERROR << "Failed to send push message." << std::endl;
        return result;
    } catch (const std::exception& e) {
        LOG_ERROR << "The app threw up: " << e.what() << std::endl;
    }

    return -1;
}
