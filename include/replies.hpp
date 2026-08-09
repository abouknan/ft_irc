/* ************************************************************************** */
/*                                                                            */
/*   replies.hpp                                                              */
/*                                                                            */
/*   Every numeric reply the server can send, in one place.                   */
/*                                                                            */
/*   A numeric always looks like:                                             */
/*     :<server> <3 digits> <target nick> [<params>] :<human text>\r\n        */
/*                                                                            */
/*   The target nick is '*' while the client has not chosen a nick yet.       */
/*   These are inline functions rather than macros so the compiler checks     */
/*   the argument types for us.                                               */
/*                                                                            */
/* ************************************************************************** */

#ifndef REPLIES_HPP
# define REPLIES_HPP

# include <string>

/* Name the server announces in every message it originates. */
# define SERVER_NAME "ircserv"

/* The channel modes we support, advertised in RPL_MYINFO. */
# define SUPPORTED_CHANNEL_MODES "itkol"

/* Common head of every numeric: ":ircserv <code> <nick> " */
inline std::string	numericHead(const std::string& code, const std::string& nick)
{
	return (":" SERVER_NAME " " + code + " " + (nick.empty() ? "*" : nick) + " ");
}

/* ---- Registration ------------------------------------------------------- */

inline std::string	rplWelcome(const std::string& nick, const std::string& prefix)
{
	return (numericHead("001", nick)
		+ ":Welcome to the ft_irc Network, " + prefix + "\r\n");
}

inline std::string	rplYourHost(const std::string& nick)
{
	return (numericHead("002", nick)
		+ ":Your host is " SERVER_NAME ", running version 1.0\r\n");
}

inline std::string	rplCreated(const std::string& nick, const std::string& date)
{
	return (numericHead("003", nick) + ":This server was created " + date + "\r\n");
}

inline std::string	rplMyInfo(const std::string& nick)
{
	return (numericHead("004", nick)
		+ SERVER_NAME " 1.0 o " SUPPORTED_CHANNEL_MODES "\r\n");
}

inline std::string	rplMotdStart(const std::string& nick)
{
	return (numericHead("375", nick) + ":- " SERVER_NAME " Message of the day -\r\n");
}

inline std::string	rplMotd(const std::string& nick, const std::string& line)
{
	return (numericHead("372", nick) + ":- " + line + "\r\n");
}

inline std::string	rplEndOfMotd(const std::string& nick)
{
	return (numericHead("376", nick) + ":End of /MOTD command\r\n");
}

/* ---- Channel state ------------------------------------------------------ */

inline std::string	rplChannelModeIs(const std::string& nick,
			const std::string& channel, const std::string& modes)
{
	return (numericHead("324", nick) + channel + " " + modes + "\r\n");
}

inline std::string	rplNoTopic(const std::string& nick, const std::string& channel)
{
	return (numericHead("331", nick) + channel + " :No topic is set\r\n");
}

inline std::string	rplTopic(const std::string& nick, const std::string& channel,
			const std::string& topic)
{
	return (numericHead("332", nick) + channel + " :" + topic + "\r\n");
}

inline std::string	rplInviting(const std::string& nick, const std::string& target,
			const std::string& channel)
{
	return (numericHead("341", nick) + target + " " + channel + "\r\n");
}

inline std::string	rplNamReply(const std::string& nick, const std::string& channel,
			const std::string& names)
{
	return (numericHead("353", nick) + "= " + channel + " :" + names + "\r\n");
}

inline std::string	rplEndOfNames(const std::string& nick, const std::string& channel)
{
	return (numericHead("366", nick) + channel + " :End of /NAMES list\r\n");
}

/* ---- Errors ------------------------------------------------------------- */

inline std::string	errNoSuchNick(const std::string& nick, const std::string& target)
{
	return (numericHead("401", nick) + target + " :No such nick/channel\r\n");
}

inline std::string	errNoSuchChannel(const std::string& nick, const std::string& channel)
{
	return (numericHead("403", nick) + channel + " :No such channel\r\n");
}

inline std::string	errCannotSendToChan(const std::string& nick, const std::string& channel)
{
	return (numericHead("404", nick) + channel + " :Cannot send to channel\r\n");
}

inline std::string	errNoRecipient(const std::string& nick, const std::string& command)
{
	return (numericHead("411", nick) + ":No recipient given (" + command + ")\r\n");
}

inline std::string	errNoTextToSend(const std::string& nick)
{
	return (numericHead("412", nick) + ":No text to send\r\n");
}

inline std::string	errUnknownCommand(const std::string& nick, const std::string& command)
{
	return (numericHead("421", nick) + command + " :Unknown command\r\n");
}

inline std::string	errNoNicknameGiven(const std::string& nick)
{
	return (numericHead("431", nick) + ":No nickname given\r\n");
}

inline std::string	errErroneusNickname(const std::string& nick, const std::string& bad)
{
	return (numericHead("432", nick) + bad + " :Erroneous nickname\r\n");
}

inline std::string	errNicknameInUse(const std::string& nick, const std::string& taken)
{
	return (numericHead("433", nick) + taken + " :Nickname is already in use\r\n");
}

inline std::string	errUserNotInChannel(const std::string& nick,
			const std::string& target, const std::string& channel)
{
	return (numericHead("441", nick) + target + " " + channel
		+ " :They aren't on that channel\r\n");
}

inline std::string	errNotOnChannel(const std::string& nick, const std::string& channel)
{
	return (numericHead("442", nick) + channel + " :You're not on that channel\r\n");
}

inline std::string	errUserOnChannel(const std::string& nick,
			const std::string& target, const std::string& channel)
{
	return (numericHead("443", nick) + target + " " + channel
		+ " :is already on channel\r\n");
}

inline std::string	errNotRegistered(const std::string& nick)
{
	return (numericHead("451", nick) + ":You have not registered\r\n");
}

inline std::string	errNeedMoreParams(const std::string& nick, const std::string& command)
{
	return (numericHead("461", nick) + command + " :Not enough parameters\r\n");
}

inline std::string	errAlreadyRegistered(const std::string& nick)
{
	return (numericHead("462", nick) + ":You may not reregister\r\n");
}

inline std::string	errPasswdMismatch(const std::string& nick)
{
	return (numericHead("464", nick) + ":Password incorrect\r\n");
}

inline std::string	errChannelIsFull(const std::string& nick, const std::string& channel)
{
	return (numericHead("471", nick) + channel + " :Cannot join channel (+l)\r\n");
}

inline std::string	errUnknownMode(const std::string& nick, char mode)
{
	return (numericHead("472", nick) + std::string(1, mode)
		+ " :is unknown mode char to me\r\n");
}

inline std::string	errInviteOnlyChan(const std::string& nick, const std::string& channel)
{
	return (numericHead("473", nick) + channel + " :Cannot join channel (+i)\r\n");
}

inline std::string	errBadChannelKey(const std::string& nick, const std::string& channel)
{
	return (numericHead("475", nick) + channel + " :Cannot join channel (+k)\r\n");
}

inline std::string	errBadChanMask(const std::string& nick, const std::string& channel)
{
	return (numericHead("476", nick) + channel + " :Bad Channel Mask\r\n");
}

inline std::string	errChanOPrivsNeeded(const std::string& nick, const std::string& channel)
{
	return (numericHead("482", nick) + channel + " :You're not channel operator\r\n");
}

/* ---- Non numeric -------------------------------------------------------- */

/* Last thing a client sees before the socket closes. */
inline std::string	errorLine(const std::string& reason)
{
	return ("ERROR :" + reason + "\r\n");
}

#endif
