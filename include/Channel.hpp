/* ************************************************************************** */
/*                                                                            */
/*   Channel.hpp                                                              */
/*                                                                            */
/*   A channel is a membership list plus the five modes the subject asks for: */
/*                                                                            */
/*     +i  invite only                                                        */
/*     +t  only operators may change the topic                                */
/*     +k  a key (password) is required to join                               */
/*     +o  operator privilege for one member                                  */
/*     +l  maximum number of members                                          */
/*                                                                            */
/*   On purpose this class only stores and answers questions. It never sends  */
/*   a numeric and never decides whether a command is allowed: the command    */
/*   handlers do that, because only they know which error to reply with.      */
/*   That keeps the rules of IRC in one place instead of two.                 */
/*                                                                            */
/*   A Channel never owns its Clients. Server does.                           */
/*                                                                            */
/* ************************************************************************** */

#ifndef CHANNEL_HPP
# define CHANNEL_HPP

# include <cstddef>
# include <set>
# include <string>

class Client;

class Channel
{
	private:

		std::string			_name;
		std::string			_topic;
		std::string			_key;
		std::size_t			_limit;

		bool				_inviteOnly;
		bool				_topicLocked;
		bool				_hasKey;
		bool				_hasLimit;
		bool				_hasTopic;

		std::set<Client*>	_members;
		std::set<Client*>	_operators;

		/* Who may walk through a +i channel once. Cleared on join. */
		std::set<Client*>	_invited;

		Channel(const Channel& other);
		Channel&	operator=(const Channel& other);

	public:

		explicit Channel(const std::string& name);
		~Channel();

		const std::string&			getName() const;
		const std::string&			getTopic() const;
		const std::string&			getKey() const;
		std::size_t					getLimit() const;

		bool						hasTopic() const;
		bool						hasKey() const;
		bool						hasLimit() const;
		bool						isInviteOnly() const;
		bool						isTopicLocked() const;
		bool						isFull() const;

		void						setTopic(const std::string& topic);
		void						setInviteOnly(bool value);
		void						setTopicLocked(bool value);
		void						setKey(const std::string& key);
		void						clearKey();
		void						setLimit(std::size_t limit);
		void						clearLimit();

		void						addMember(Client& client);
		void						removeMember(Client& client);
		bool						isMember(Client& client) const;
		std::size_t					size() const;
		bool						isEmpty() const;
		const std::set<Client*>&	getMembers() const;

		void						addOperator(Client& client);
		void						removeOperator(Client& client);
		bool						isOperator(Client& client) const;

		void						addInvite(Client& client);
		void						removeInvite(Client& client);
		bool						isInvited(Client& client) const;

		/* "+itk" - the flags currently set, without their arguments. */
		std::string					modeString() const;

		/* "secretkey 10" - the arguments of +k and +l, in that order. Only
		   shown to members: the key must not leak to the outside. */
		std::string					modeArguments() const;

		/* "@alice bob carol" for RPL_NAMREPLY. */
		std::string					namesList() const;

		/* Queues a ready made line into every member except `except`
		   (usually the author of the action, who is handled separately). */
		void						broadcast(const std::string& message, Client* except);
};

#endif
