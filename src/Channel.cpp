/* ************************************************************************** */
/*                                                                            */
/*   Channel.cpp                                                              */
/*                                                                            */
/* ************************************************************************** */

#include "Channel.hpp"
#include "Client.hpp"
#include "Utils.hpp"

Channel::Channel(const std::string& name)
	: _name(name),
	  _topic(),
	  _key(),
	  _limit(0),
	  _inviteOnly(false),
	  _topicLocked(false),
	  _hasKey(false),
	  _hasLimit(false),
	  _hasTopic(false),
	  _members(),
	  _operators(),
	  _invited()
{
}

Channel::~Channel()
{
}

const std::string&	Channel::getName() const
{
	return (_name);
}

const std::string&	Channel::getTopic() const
{
	return (_topic);
}

const std::string&	Channel::getKey() const
{
	return (_key);
}

std::size_t	Channel::getLimit() const
{
	return (_limit);
}

bool	Channel::hasTopic() const
{
	return (_hasTopic);
}

bool	Channel::hasKey() const
{
	return (_hasKey);
}

bool	Channel::hasLimit() const
{
	return (_hasLimit);
}

bool	Channel::isInviteOnly() const
{
	return (_inviteOnly);
}

bool	Channel::isTopicLocked() const
{
	return (_topicLocked);
}

bool	Channel::isFull() const
{
	return (_hasLimit && _members.size() >= _limit);
}

void	Channel::setTopic(const std::string& topic)
{
	_topic = topic;
	/* An empty TOPIC clears it, which is how a client wipes a topic. */
	_hasTopic = !topic.empty();
}

void	Channel::setInviteOnly(bool value)
{
	_inviteOnly = value;
}

void	Channel::setTopicLocked(bool value)
{
	_topicLocked = value;
}

void	Channel::setKey(const std::string& key)
{
	_key = key;
	_hasKey = true;
}

void	Channel::clearKey()
{
	_key.clear();
	_hasKey = false;
}

void	Channel::setLimit(std::size_t limit)
{
	_limit = limit;
	_hasLimit = true;
}

void	Channel::clearLimit()
{
	_limit = 0;
	_hasLimit = false;
}

void	Channel::addMember(Client& client)
{
	_members.insert(&client);
	/* The invite was a one shot ticket. */
	_invited.erase(&client);
}

void	Channel::removeMember(Client& client)
{
	_members.erase(&client);
	_operators.erase(&client);
	_invited.erase(&client);
}

bool	Channel::isMember(Client& client) const
{
	return (_members.find(&client) != _members.end());
}

std::size_t	Channel::size() const
{
	return (_members.size());
}

bool	Channel::isEmpty() const
{
	return (_members.empty());
}

const std::set<Client*>&	Channel::getMembers() const
{
	return (_members);
}

void	Channel::addOperator(Client& client)
{
	_operators.insert(&client);
}

void	Channel::removeOperator(Client& client)
{
	_operators.erase(&client);
}

bool	Channel::isOperator(Client& client) const
{
	return (_operators.find(&client) != _operators.end());
}

void	Channel::addInvite(Client& client)
{
	_invited.insert(&client);
}

void	Channel::removeInvite(Client& client)
{
	_invited.erase(&client);
}

bool	Channel::isInvited(Client& client) const
{
	return (_invited.find(&client) != _invited.end());
}

std::string	Channel::modeString() const
{
	std::string	modes("+");

	if (_inviteOnly)
		modes += "i";
	if (_topicLocked)
		modes += "t";
	if (_hasKey)
		modes += "k";
	if (_hasLimit)
		modes += "l";
	return (modes);
}

std::string	Channel::modeArguments() const
{
	std::string	arguments;

	/* Order must match the order of the flags in modeString(). */
	if (_hasKey)
		arguments += _key;
	if (_hasLimit)
	{
		if (!arguments.empty())
			arguments += " ";
		arguments += toString(static_cast<long>(_limit));
	}
	return (arguments);
}

std::string	Channel::namesList() const
{
	std::string								names;
	std::set<Client*>::const_iterator		it = _members.begin();

	while (it != _members.end())
	{
		if (!names.empty())
			names += " ";
		if (_operators.find(*it) != _operators.end())
			names += "@";
		names += (*it)->getNick();
		++it;
	}
	return (names);
}

void	Channel::broadcast(const std::string& message, Client* except)
{
	std::set<Client*>::iterator	it = _members.begin();

	while (it != _members.end())
	{
		if (*it != except)
			(*it)->queue(message);
		++it;
	}
}
