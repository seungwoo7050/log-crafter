#include "IRCCommandParser.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <regex>
#include <iomanip>

// [SEQUENCE: 600] Static command validation map
const std::map<std::string, bool> IRCCommandParser::validCommands_ = {
    // Core IRC commands
    {"NICK", true}, {"USER", true}, {"PASS", true}, {"JOIN", true},
    {"PART", true}, {"PRIVMSG", true}, {"NOTICE", true}, {"QUIT", true},
    {"PING", true}, {"PONG", true}, {"MODE", true}, {"TOPIC", true},
    {"NAMES", true}, {"LIST", true}, {"KICK", true}, {"WHO", true},
    {"WHOIS", true}, {"MOTD", true}, {"VERSION", true}, {"TIME", true},
    {"INFO", true}, {"AWAY", true}, {"WHOWAS", true}, {"ISON", true},
    // LogCrafter custom commands
    {"LOGQUERY", true}, {"LOGFILTER", true}, {"LOGSTREAM", true}, {"LOGSTATS", true}
};

// [SEQUENCE: 601] Reply code descriptions
const std::map<int, std::string> IRCCommandParser::replyDescriptions_ = {
    {RPL_WELCOME, "Welcome to the Internet Relay Network"},
    {RPL_YOURHOST, "Your host is %s, running version %s"},
    {RPL_CREATED, "This server was created %s"},
    {RPL_MYINFO, "Server info"},
    {ERR_NOSUCHNICK, "No such nick/channel"},
    {ERR_NOSUCHCHANNEL, "No such channel"},
    {ERR_CANNOTSENDTOCHAN, "Cannot send to channel"},
    {ERR_NICKNAMEINUSE, "Nickname is already in use"},
    {ERR_NOTREGISTERED, "You have not registered"},
    {ERR_NEEDMOREPARAMS, "Not enough parameters"},
    {ERR_ALREADYREGISTRED, "You may not reregister"},
    {ERR_CHANOPRIVSNEEDED, "You're not channel operator"}
};

// [SEQUENCE: 602] Main parse function - converts raw IRC line to command structure
IRCCommandParser::IRCCommand IRCCommandParser::parse(const std::string& line) {
    IRCCommand cmd;
    
    if (line.empty()) {
        return cmd;
    }
    
    std::string workingLine = line;
    
    // [SEQUENCE: 603] Remove trailing CRLF
    while (!workingLine.empty() && 
           (workingLine.back() == '\r' || workingLine.back() == '\n')) {
        workingLine.pop_back();
    }
    
    // [SEQUENCE: 604] Extract optional prefix
    cmd.prefix = extractPrefix(workingLine);
    
    // [SEQUENCE: 605] Extract command
    cmd.command = extractCommand(workingLine);
    
    // [SEQUENCE: 606] Extract parameters
    cmd.params = extractParams(workingLine);
    
    // [SEQUENCE: 607] Handle trailing parameter
    if (!cmd.params.empty() && !cmd.params.back().empty() && cmd.params.back()[0] == ':') {
        cmd.trailing = cmd.params.back().substr(1);
        cmd.params.pop_back();
    }
    
    return cmd;
}

// [SEQUENCE: 608] Extract prefix from IRC message
std::string IRCCommandParser::extractPrefix(std::string& line) {
    if (line.empty() || line[0] != ':') {
        return "";
    }
    
    size_t spacePos = line.find(' ');
    if (spacePos == std::string::npos) {
        std::string prefix = line.substr(1);
        line.clear();
        return prefix;
    }
    
    std::string prefix = line.substr(1, spacePos - 1);
    line = line.substr(spacePos + 1);
    return prefix;
}

// [SEQUENCE: 609] Extract command from IRC message
std::string IRCCommandParser::extractCommand(std::string& line) {
    if (line.empty()) {
        return "";
    }
    
    size_t spacePos = line.find(' ');
    if (spacePos == std::string::npos) {
        std::string command = line;
        line.clear();
        return toUpper(command);
    }
    
    std::string command = line.substr(0, spacePos);
    line = line.substr(spacePos + 1);
    return toUpper(command);
}

// [SEQUENCE: 610] Extract parameters from IRC message
std::vector<std::string> IRCCommandParser::extractParams(std::string& line) {
    std::vector<std::string> params;
    
    while (!line.empty()) {
        if (line[0] == ':') {
            // [SEQUENCE: 611] Trailing parameter - rest of line
            params.push_back(line);
            break;
        }
        
        size_t spacePos = line.find(' ');
        if (spacePos == std::string::npos) {
            params.push_back(line);
            break;
        }
        
        params.push_back(line.substr(0, spacePos));
        line = line.substr(spacePos + 1);
    }
    
    return params;
}

// [SEQUENCE: 612] Format numeric reply
std::string IRCCommandParser::formatReply(const std::string& serverName,
                                          const std::string& nick,
                                          int code,
                                          const std::string& params) {
    std::ostringstream oss;
    oss << ":" << serverName << " " << std::setfill('0') << std::setw(3) << code 
        << " " << nick << " " << params;
    return oss.str();
}

// [SEQUENCE: 613] Format server message
std::string IRCCommandParser::formatServerMessage(const std::string& serverName,
                                                  const std::string& command,
                                                  const std::string& params) {
    return ":" + serverName + " " + command + " " + params;
}

// [SEQUENCE: 614] Format user message
std::string IRCCommandParser::formatUserMessage(const std::string& nick,
                                                const std::string& user,
                                                const std::string& host,
                                                const std::string& command,
                                                const std::string& target,
                                                const std::string& message) {
    std::ostringstream oss;
    oss << ":" << nick << "!" << user << "@" << host 
        << " " << command << " " << target;
    
    if (!message.empty()) {
        oss << " :" << message;
    }
    
    return oss.str();
}

// [SEQUENCE: 615] Validate nickname according to IRC rules
bool IRCCommandParser::isValidNickname(const std::string& nick) {
    if (nick.empty() || nick.length() > 9) {
        return false;
    }
    
    // [SEQUENCE: 616] First character must be letter or special
    char first = nick[0];
    if (!std::isalpha(first) && first != '_' && first != '[' && 
        first != ']' && first != '{' && first != '}' && 
        first != '\\' && first != '|' && first != '^') {
        return false;
    }
    
    // [SEQUENCE: 617] Rest can be letters, digits, special chars, hyphen
    for (size_t i = 1; i < nick.length(); ++i) {
        char c = nick[i];
        if (!std::isalnum(c) && c != '_' && c != '-' && 
            c != '[' && c != ']' && c != '{' && c != '}' && 
            c != '\\' && c != '|' && c != '^') {
            return false;
        }
    }
    
    return true;
}

// [SEQUENCE: 618] Validate channel name
bool IRCCommandParser::isValidChannelName(const std::string& channel) {
    if (channel.empty() || channel.length() > 50) {
        return false;
    }
    
    // [SEQUENCE: 619] Must start with # or &
    if (channel[0] != '#' && channel[0] != '&') {
        return false;
    }
    
    // [SEQUENCE: 620] Cannot contain spaces, commas, or control chars
    for (char c : channel) {
        if (c == ' ' || c == ',' || c == '\x07' || c < 32) {
            return false;
        }
    }
    
    return true;
}

// [SEQUENCE: 621] Check if command is valid
bool IRCCommandParser::isValidCommand(const std::string& command) {
    return validCommands_.find(toUpper(command)) != validCommands_.end();
}

// [SEQUENCE: 622] Convert string to uppercase
std::string IRCCommandParser::toUpper(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

// [SEQUENCE: 623] Split comma-separated channels
std::vector<std::string> IRCCommandParser::splitChannels(const std::string& channels) {
    std::vector<std::string> result;
    std::stringstream ss(channels);
    std::string channel;
    
    while (std::getline(ss, channel, ',')) {
        if (!channel.empty()) {
            result.push_back(channel);
        }
    }
    
    return result;
}

// [SEQUENCE: 624] Split space-separated parameters
std::vector<std::string> IRCCommandParser::splitParams(const std::string& params) {
    std::vector<std::string> result;
    std::istringstream iss(params);
    std::string param;
    
    while (iss >> param) {
        result.push_back(param);
    }
    
    return result;
}

// [SEQUENCE: 625] Get human-readable reply description
std::string IRCCommandParser::getReplyDescription(int code) {
    auto it = replyDescriptions_.find(code);
    if (it != replyDescriptions_.end()) {
        return it->second;
    }
    return "Unknown reply code";
}