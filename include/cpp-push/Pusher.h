#pragma once

#include <string>
#include <string_view>
#include <filesystem>
#include <string_view>
#include <ranges>
#include <variant>

#include <boost/asio.hpp>

namespace jgaa::cpp_push {

struct Config {

    struct Google {
        /*! configFile The path to the configuration file. This is the service file you downloaded when
           you created the firebase project for push notification to your Android app.
        */
        std::filesystem::path config_file{};
        int jwt_ttl_minutes{45}; // Time to live for the JWT token in minutes
        int jwt_refresh_minutes{3}; // Refresh the JWT token n minutes before the existing token expires
    };

    Google google;
};

/*! Structure representing a notification message.
 * This structure contains the title, body, and sound of the notification.
 *
 * For platform specific fields, use the Google or Apple specializations
 */
struct Notification {
    std::string_view title;
    std::string_view body;
    std::string_view sound;
    std::string_view icon;
};

struct PushMessage {
    enum class PushType {
        DATA,
        NOTIFICATION,
    };

    using tokens_t = std::variant<std::string_view, std::span<std::string_view>>;
    using data_t = std::vector<std::pair<std::string_view, std::string_view>>;

    tokens_t to;    // 1-many devices across platforms
    data_t data;    // always delivered
    PushType type{PushType::DATA}; // default to data push
    std::optional<Notification> notification;

    // zero‐allocation view over either one token or many
    struct tokens_view {
        // store a ref, not a copy
        tokens_t const& tok;

        // ctor from the variant reference
        tokens_view(tokens_t const& t) noexcept
            : tok(t)
        {}

        // forward iterator over string_view
        struct iterator {
            const std::string_view* cur;
            const std::string_view* end;

            using value_type        = std::string_view;
            using reference         = std::string_view;
            using iterator_category = std::forward_iterator_tag;

            reference operator*()   const noexcept { return *cur; }
            iterator& operator++()  noexcept { ++cur; return *this; }
            bool operator!=(iterator o) const noexcept { return cur != o.cur; }
        };

        iterator begin() const noexcept {
            return std::visit([](auto const& c) -> iterator {
                using C = std::decay_t<decltype(c)>;
                if constexpr (std::is_same_v<C, std::string_view>) {
                    // single‐element: point at it, and one past it
                    const std::string_view* p = &c;
                    return { p, p + 1 };
                } else {
                    // span case: data() is contiguous
                    return { c.data(), c.data() + c.size() };
                }
            }, tok);
        }

        iterator end() const noexcept {
            return std::visit([](auto const& c) -> iterator {
                using C = std::decay_t<decltype(c)>;
                if constexpr (std::is_same_v<C, std::string_view>) {
                    const std::string_view* p = &c;
                    return { p + 1, p + 1 };
                } else {
                    return { c.data() + c.size(), c.data() + c.size() };
                }
            }, tok);
        }
    };
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
        Result(bool success, const std::string& errorMessage, unsigned int num_successful)
            : success_(success), num_successful_{num_successful}, message_(errorMessage) {}

        Result(unsigned int num_successful)
            : success_(true), num_successful_{num_successful} {}

        bool ok() const noexcept {
            return success_;
        }

        std::string_view message() const noexcept {
            return message_;
        }

        auto numSuccessfulPushes() const noexcept {
            return num_successful_;
        }

    private:
        /*! Indicates whether the push operation was successful. */
        bool success_{false};
        unsigned int num_successful_{0}; // Number of successful pushes, if applicable

        /*! Contains the error message if the push operation failed. */
        std::string message_;
    };

    /*! Virtual destructor to ensure proper cleanup of derived classes. */
    virtual ~Pusher() = default;

    /*! Pure virtual function to push data.
     * @param pm The data to be pushed.
     * @return Result. Indicates success or failure of the push operation.
     */
    [[nodiscard]] virtual boost::asio::awaitable<Result> push(const PushMessage& pm) = 0;

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
std::shared_ptr<Pusher> createPusherForGoogle(const Config& config, boost::asio::io_context& ctx);

} // ns

