#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <map>

// [SEQUENCE: 543] IRC Command Parser - handles IRC protocol parsing
class IRCCommandParser {
public:
    // [SEQUENCE: 544] Parsed IRC command structure
    struct IRCCommand {
        std::string prefix;                    // Optional prefix (:nick!user@host)
        std::string command;                   // Command (PRIVMSG, JOIN, etc.)
        std::vector<std::string> params;       // Command parameters
        std::string trailing;                  // Trailing parameter after :
        
        // [SEQUENCE: 545] Helper to get full parameter at index
        std::string getParam(size_t index) const {
            return (index < params.size()) ? params[index] : "";
        }
    };
    
    // [SEQUENCE: 546] IRC numeric reply codes (RFC 2812)
    enum ReplyCode {
        // Connection replies
        RPL_WELCOME = 001,
        RPL_YOURHOST = 002,
        RPL_CREATED = 003,
        RPL_MYINFO = 004,
        RPL_ISUPPORT = 005,
        
        // Command replies
        RPL_UMODEIS = 221,
        RPL_CHANNELMODEIS = 324,
        RPL_NOTOPIC = 331,
        RPL_TOPIC = 332,
        RPL_TOPICWHOTIME = 333,
        RPL_INVITING = 341,
        RPL_NAMREPLY = 353,
        RPL_ENDOFNAMES = 366,
        RPL_ENDOFBANLIST = 368,
        RPL_MOTD = 372,
        RPL_MOTDSTART = 375,
        RPL_ENDOFMOTD = 376,
        
        // Error replies
        ERR_NOSUCHNICK = 401,
        ERR_NOSUCHSERVER = 402,
        ERR_NOSUCHCHANNEL = 403,
        ERR_CANNOTSENDTOCHAN = 404,
        ERR_TOOMANYCHANNELS = 405,
        ERR_NOTEXTTOSEND = 412,
        ERR_UNKNOWNCOMMAND = 421,
        ERR_NONICKNAMEGIVEN = 431,
        ERR_ERRONEUSNICKNAME = 432,
        ERR_NICKNAMEINUSE = 433,
        ERR_USERNOTINCHANNEL = 441,
        ERR_NOTONCHANNEL = 442,
        ERR_USERONCHANNEL = 443,
        ERR_NOTREGISTERED = 451,
        ERR_NEEDMOREPARAMS = 461,
        ERR_ALREADYREGISTRED = 462,
        ERR_CHANNELISFULL = 471,
        ERR_INVITEONLYCHAN = 473,
        ERR_BANNEDFROMCHAN = 474,
        ERR_BADCHANNELKEY = 475,
        ERR_CHANOPRIVSNEEDED = 482,
        
        // LogCrafter custom replies (900+)
        RPL_LOGQUERY = 900,
        RPL_LOGFILTER = 901,
        RPL_LOGSTREAM = 902,
        ERR_LOGQUERYFAILED = 950,
        ERR_LOGFILTERFAILED = 951
    };
    
    // [SEQUENCE: 547] Parse raw IRC message
    static IRCCommand parse(const std::string& line);
    
    // [SEQUENCE: 548] Format numeric reply
    static std::string formatReply(const std::string& serverName, 
                                   const std::string& nick,
                                   int code, 
                                   const std::string& params);
    
    // [SEQUENCE: 549] Format generic server message
    static std::string formatServerMessage(const std::string& serverName,
                                           const std::string& command,
                                           const std::string& params);
    
    // [SEQUENCE: 550] Format user message (PRIVMSG, NOTICE, etc.)
    static std::string formatUserMessage(const std::string& nick,
                                         const std::string& user,
                                         const std::string& host,
                                         const std::string& command,
                                         const std::string& target,
                                         const std::string& message);
    
    // [SEQUENCE: 551] Validate IRC protocol elements
    static bool isValidNickname(const std::string& nick);
    static bool isValidChannelName(const std::string& channel);
    static bool isValidCommand(const std::string& command);
    
    // [SEQUENCE: 552] IRC protocol helpers
    static std::string toUpper(const std::string& str);
    static std::vector<std::string> splitChannels(const std::string& channels);
    static std::vector<std::string> splitParams(const std::string& params);
    
    // [SEQUENCE: 553] Get human-readable reply descriptions
    static std::string getReplyDescription(int code);
    
private:
    // [SEQUENCE: 554] Parsing helpers
    static std::string extractPrefix(std::string& line);
    static std::string extractCommand(std::string& line);
    static std::vector<std::string> extractParams(std::string& line);
    
    // [SEQUENCE: 555] Static maps for command validation
    static const std::map<std::string, bool> validCommands_;
    static const std::map<int, std::string> replyDescriptions_;
};