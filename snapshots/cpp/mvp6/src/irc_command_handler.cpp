/*
 * Sequence: SEQ0066
 * Track: C++
 * MVP: mvp6
 * Change: Implement the LogCrafter IRC command helpers for queries, log streaming, and stats reporting.
 * Tests: smoke_cpp_mvp6_irc
 */
#include "irc_command_handler.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

#include "irc_command_parser.hpp"
#include "query_parser.hpp"

namespace logcrafter::cpp {

namespace {

std::string lowercase(const std::string &value) {
    std::string result = value;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return result;
}

std::string build_filter_channel_name(const std::string &nickname) {
    std::string slug = lowercase(nickname);
    for (char &ch : slug) {
        if (!std::isalnum(static_cast<unsigned char>(ch))) {
            ch = '-';
        }
    }
    if (slug.empty()) {
        slug = "anon";
    }
    if (slug.size() > 12) {
        slug.resize(12);
    }
    return "#logs-filter-" + slug;
}

} // namespace

IRCCommandHandler::IRCCommandHandler(LogBuffer &buffer, IRCChannelManager &channels)
    : buffer_(buffer), channels_(channels), stats_callback_() {}

void IRCCommandHandler::set_stats_callback(StatsCallback cb) { stats_callback_ = std::move(cb); }

IRCCommandResult IRCCommandHandler::handle_privmsg(int client_fd,
                                                   const std::string &nickname,
                                                   const std::string &target,
                                                   const std::string &message) const {
    IRCCommandResult result;
    const std::string trimmed = trim(message);
    if (trimmed.empty() || trimmed.front() != '!') {
        return result;
    }

    const std::size_t space = trimmed.find(' ');
    const std::string verb = lowercase(space == std::string::npos ? trimmed.substr(1) : trimmed.substr(1, space - 1));
    const std::string arguments = space == std::string::npos ? std::string() : trimmed.substr(space + 1);

    if (verb == "query") {
        result = handle_query_command(client_fd, nickname, arguments);
    } else if (verb == "logstream") {
        result = handle_logstream_command(client_fd, nickname, arguments);
    } else if (verb == "logfilter") {
        result = handle_logfilter_command(client_fd, nickname, arguments);
    } else if (verb == "logstats") {
        result = handle_logstats_command(nickname);
    } else if (verb == "help") {
        result = handle_help_command(nickname);
    } else {
        result.handled = true;
        result.replies.push_back({IRCCommandReply::Type::Notice, nickname,
                                  "Unknown command. Try !help for usage."});
    }

    if (result.handled && result.replies.empty()) {
        result.replies.push_back({IRCCommandReply::Type::Notice, nickname,
                                  "Command processed."});
    }

    return result;
}

IRCCommandResult IRCCommandHandler::handle_query_command(int client_fd,
                                                         const std::string &nickname,
                                                         const std::string &arguments) const {
    IRCCommandResult result;
    result.handled = true;

    QueryRequest request;
    std::string error;
    if (!parse_query_arguments(arguments, request, error)) {
        if (error.empty()) {
            error = "Invalid query.";
        }
        result.replies.push_back({IRCCommandReply::Type::Notice, nickname, error});
        return result;
    }

    std::vector<std::string> matches = buffer_.execute_query(request);
    constexpr std::size_t kMaxLines = 5;
    std::ostringstream oss;
    oss << "!query matched " << matches.size() << " entr" << (matches.size() == 1 ? 'y' : 'i') << 's';
    if (matches.size() > kMaxLines) {
        oss << " (showing " << kMaxLines << ")";
    }
    result.replies.push_back({IRCCommandReply::Type::Notice, nickname, oss.str()});

    std::size_t emitted = 0;
    for (const std::string &line : matches) {
        if (emitted++ >= kMaxLines) {
            break;
        }
        result.replies.push_back({IRCCommandReply::Type::Notice, nickname, line});
    }

    if (matches.empty()) {
        result.replies.push_back({IRCCommandReply::Type::Notice, nickname,
                                  "No buffered logs match the supplied filters."});
    }

    (void)client_fd;
    return result;
}

IRCCommandResult IRCCommandHandler::handle_logstream_command(int client_fd,
                                                             const std::string &nickname,
                                                             const std::string &arguments) const {
    IRCCommandResult result;
    result.handled = true;

    const std::string lowered = lowercase(trim(arguments));
    if (lowered.empty()) {
        result.replies.push_back({IRCCommandReply::Type::Notice, nickname,
                                  "Usage: !logstream <all|error|warning|info|debug|off>"});
        return result;
    }

    if (lowered == "off") {
        result.part_channels = {"#logs-all", "#logs-error", "#logs-warning", "#logs-info", "#logs-debug"};
        result.replies.push_back({IRCCommandReply::Type::Notice, nickname,
                                  "Log streaming disabled. Use !logstream all to resume."});
        return result;
    }

    std::vector<std::string> channels;
    if (lowered == "all") {
        channels.push_back("#logs-all");
    } else if (lowered == "error" || lowered == "warning" || lowered == "info" || lowered == "debug") {
        channels.push_back("#logs-" + lowered);
    } else {
        result.replies.push_back({IRCCommandReply::Type::Notice, nickname,
                                  "Unknown stream. Valid options: all, error, warning, info, debug, off."});
        return result;
    }

    result.join_channels = channels;
    result.replies.push_back({IRCCommandReply::Type::Notice, nickname,
                              "Joined log stream channel(s)."});
    (void)client_fd;
    return result;
}

IRCCommandResult IRCCommandHandler::handle_logfilter_command(int client_fd,
                                                             const std::string &nickname,
                                                             const std::string &arguments) const {
    IRCCommandResult result;
    result.handled = true;

    const std::string trimmed = trim(arguments);
    if (trimmed.empty()) {
        result.replies.push_back({IRCCommandReply::Type::Notice, nickname,
                                  "Usage: !logfilter <keyword>[,<keyword>...] or !logfilter off"});
        return result;
    }

    if (lowercase(trimmed) == "off") {
        const std::string channel = build_filter_channel_name(nickname);
        result.part_channels.push_back(channel);
        result.replies.push_back({IRCCommandReply::Type::Notice, nickname,
                                  "Removed custom filter channel."});
        return result;
    }

    const std::vector<std::string> tokens = split_list(trimmed);
    if (tokens.empty()) {
        result.replies.push_back({IRCCommandReply::Type::Notice, nickname,
                                  "Provide at least one keyword to filter on."});
        return result;
    }

    const std::string channel_name = build_filter_channel_name(nickname);
    const std::string topic = "Custom log filter for " + nickname;
    channels_.ensure_filter_channel(channel_name, topic, [tokens](const std::string &message) {
        const std::string lowered_message = lowercase(message);
        for (const std::string &token : tokens) {
            if (lowered_message.find(token) == std::string::npos) {
                return false;
            }
        }
        return true;
    });
    result.join_channels.push_back(channel_name);
    result.replies.push_back({IRCCommandReply::Type::Notice, nickname,
                              "Joined custom filter channel " + channel_name + "."});

    (void)client_fd;
    return result;
}

IRCCommandResult IRCCommandHandler::handle_logstats_command(const std::string &nickname) const {
    IRCCommandResult result;
    result.handled = true;

    std::string stats_text = stats_callback_ ? stats_callback_() : std::string();
    if (stats_text.empty()) {
        stats_text = "No statistics available.";
    }
    const auto channels = channels_.stats();
    if (!channels.empty()) {
        std::ostringstream oss;
        oss << stats_text << " | channels=" << channels.size();
        const std::size_t preview = std::min<std::size_t>(channels.size(), 3);
        oss << " [";
        for (std::size_t i = 0; i < preview; ++i) {
            if (i > 0) {
                oss << ", ";
            }
            oss << channels[i].name << ':' << channels[i].members;
        }
        if (channels.size() > preview) {
            oss << ", ...";
        }
        oss << ']';
        stats_text = oss.str();
    }
    result.replies.push_back({IRCCommandReply::Type::Notice, nickname, stats_text});
    return result;
}

IRCCommandResult IRCCommandHandler::handle_help_command(const std::string &nickname) const {
    IRCCommandResult result;
    result.handled = true;
    result.replies.push_back({IRCCommandReply::Type::Notice, nickname,
                              "IRC helpers: !query <args>, !logstream <level>, !logfilter <kw>,"
                              " !logfilter off, !logstats"});
    return result;
}

std::string IRCCommandHandler::trim(const std::string &value) {
    std::size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }
    std::size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return value.substr(start, end - start);
}

std::vector<std::string> IRCCommandHandler::split_list(const std::string &value) {
    std::vector<std::string> tokens;
    std::string current;
    for (char ch : value) {
        if (ch == ',') {
            const std::string trimmed = trim(current);
            if (!trimmed.empty()) {
                tokens.push_back(lowercase(trimmed));
            }
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    const std::string trimmed = trim(current);
    if (!trimmed.empty()) {
        tokens.push_back(lowercase(trimmed));
    }
    return tokens;
}

} // namespace logcrafter::cpp

