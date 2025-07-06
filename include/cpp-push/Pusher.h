#pragma once

#include <string>
#include <string_view>
#include <filesystem>

#include <boost/asio.hpp>

namespace jgaa::cpp_push {

struct Config {

    struct Google {
        /*! configFile The path to the configuration file. This is the service file you downloaded when
           you created the firebase project for push notification to your Android app.
        */
        std::filesystem::path config_file{};
        int jwt_refresh_minutes{30}; // minutes after which the JWT token is refreshed
    };

    Google google;
};

/*! Base class for pushing data to a remote server.
 * This class serves as a base for implementing various push mechanisms.
 *
 * Note that this class will try to push the message immediately and returns an error
 * state if that is not possible. It does not provide a queueing mechanism for messages.
 */
class Pusher {
public:
    struct Result {
        /*! Default constructor initializing success to false. */
        Result() = default;

        /*! Constructor initializing success and error message.
         * @param success Indicates if the push was successful.
         * @param errorMessage The error message if any.
         */
        Result(bool success, const std::string& errorMessage)
            : success_(success), message_(errorMessage) {}

        Result(bool success)
            : success_(success) {}

        bool ok() const noexcept {
            return success_;
        }

        std::string_view message() const noexcept {
            return message_;
        }

    private:
        /*! Indicates whether the push operation was successful. */
        bool success_{false};

        /*! Contains the error message if the push operation failed. */
        std::string message_;
    };

    /*! Virtual destructor to ensure proper cleanup of derived classes. */
    virtual ~Pusher() = default;

    /*! Pure virtual function to push data.
     * @param data The data to be pushed.
     * @return True if the push was successful, false otherwise.
     */
    [[nodiscard]] virtual boost::asio::awaitable<Result> push(const std::string& data) = 0;

    /*! Pure virtual function to check if the pusher is ready.
     * @return True if the pusher is ready, false otherwise.
     */
    virtual bool isReady() const = 0;

    /*! Pure virtual function to stop the pusher.
     */
    virtual void stop() = 0;
};

/*! Factory function
 *
 *  Creates a Pusher instance for Google Firebase Cloud Messaging (FCM).
 *  @param config The configuration object containing necessary parameters.
 *
 *  You will normally only need one Pusher instance for your application.
 *  The Pusher instrance can handle many simultaneous requests. It use an
 *  internal worker-thread to handle the requests asynchronously.
 */
std::shared_ptr<Pusher> createPusherForGoggle(const Config& config, boost::asio::io_context& ctx);

} // ns

