/* ************************************************************************** */
/*                                                                            */
/*   Utils.hpp                                                                */
/*                                                                            */
/*   Small free functions shared by every part of the server.                 */
/*   No state, no allocation of its own: pure helpers.                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef UTILS_HPP
# define UTILS_HPP

# include <string>
# include <vector>

/* IRC is case insensitive, but not in the ASCII sense: RFC 1459 section 2.2
   says that '[' ']' and '\' are the upper case versions of '{' '}' and '|'.
   Every nickname and channel name comparison goes through this function so
   that "Bob" and "bob" are the same user everywhere in the server. */
std::string					toIrcLower(const std::string& text);

/* IRC lets a client target several destinations at once:
   "PRIVMSG #a,#b :hi" or "JOIN #a,#b key1,key2". Splits on a separator and
   drops empty fields. */
std::vector<std::string>	split(const std::string& text, char separator);

/* C++98 has no std::to_string, and numerics need numbers in their text. */
std::string					toString(long value);

/* RFC 2812 2.3.1: a nickname starts with a letter or one of "[]\`_^{|}" and
   continues with those, digits or '-'. */
bool						isValidNick(const std::string& nick);

/* RFC 2812 2.3.1: a channel name starts with '#' or '&' and contains no
   space, no comma, no BEL and no NUL. */
bool						isValidChannelName(const std::string& name);

#endif
