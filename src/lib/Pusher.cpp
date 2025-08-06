
#include <boost/json.hpp>
#include <boost/url.hpp>

#include "cpp-push/logging.h"
#include "cpp-push/Pusher.h"

namespace json = boost::json;
using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std;

ostream& operator << (ostream& out, const jgaa::cpp_push::Pusher::State& state) {
    constexpr auto states = to_array<string_view>({
        string_view{"STARTING"},
        string_view{"AVAILABLE"},
        string_view{"ERROR"},
        string_view{"STOPPING"},
        string_view{"STOPPED"}
    });

    return out << states.at(static_cast<size_t>(state));
}

namespace jgaa::cpp_push {

Pusher::Pusher(const Config &config)
    : config_(config)
{

}

void Pusher::setState(State state)
{
    if (state != state_) {
        LOG_DEBUG_N << "state changed from "
                    << state_ << " to " << state;
        state_ = state;
    }
}




} // ns
