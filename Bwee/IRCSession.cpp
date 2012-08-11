#include "IRCSession.h"

IRCSession::IRCSession(std::string pServer, uint32 pPort)
{
	// Setup the message mappings
	ADD_MESSAGEHANDLER(MESSAGE_PING, &IRCSession::HandlePing);
	ADD_MESSAGEHANDLER(MESSAGE_RAWNUMERIC_001, &IRCSession::Handle001);
	ADD_MESSAGEHANDLER(MESSAGE_PRIVMSG, &IRCSession::HandlePrivmsg);
	ADD_MESSAGEHANDLER(MESSAGE_RPL_TOPIC, &IRCSession::HandleReplyTopic);
	ADD_MESSAGEHANDLER(MESSAGE_RPL_NOTOPIC, &IRCSession::HandleReplyTopic);
	ADD_MESSAGEHANDLER(MESSAGE_TOPIC, &IRCSession::HandleReplyTopic);			// NOT RFC 1459 COMPLIANT.

	m_scriptInterface = new ScriptInterface(this);
	m_hasQuit = false;
	m_socket = new SimpleSocket(pServer, pPort);

	Update();
}

IRCSession::~IRCSession()
{
	if(!m_hasQuit)
		Quit(Util::getVersionString().c_str());

	delete m_socket;
	m_socket = NULL;
}

void IRCSession::Parse(std::string pMessage)
{
	std::cout << pMessage << std::endl;

	/*
	 * Any message we receive will be in the following format:
	 *	:<prefix> <command> <params> :<trailing>
	 * >> prefix and trailing parts are optional. If the prefix or trailing part is missing, there will also be missing ':'
	 */
	IRCMessage message;

	// We need to determine if the prefix is present or not.
	size_t prefixOffset = 0;
	if( pMessage[0] == ':' )
	{
		// Read in the prefix
		prefixOffset = pMessage.find(' ');
		message.rawPrefix = pMessage.substr(1, prefixOffset-1);
		message.prefix = new IRCMessagePrefix(message.rawPrefix);

		// increment prefixOffset only if there was a prefix, so that it points to the next word
		++prefixOffset; 
	}

	size_t postCommandSpace = pMessage.find(' ', prefixOffset);
	message.command = pMessage.substr(prefixOffset, postCommandSpace-prefixOffset);
	size_t trailingMarker = pMessage.find(" :", postCommandSpace);

	// Read in the parameters
	message.rawParams = pMessage.substr(postCommandSpace+1, trailingMarker-postCommandSpace-1);
	size_t lastSpace = postCommandSpace;
	while( lastSpace != string::npos && lastSpace < trailingMarker )
	{
		size_t nextSpace = pMessage.find(' ', lastSpace+1);
		std::string param = pMessage.substr(lastSpace+1, nextSpace-lastSpace-1);
		
		message.params.push_back(param);

		lastSpace = nextSpace;
	}
	
	if( trailingMarker != string::npos )
		message.trailing = pMessage.substr(trailingMarker+2);

	HandleMessage(&message);
}

void IRCSession::HandleMessage(IRCMessage* pMessage)
{
	MessageHandlerMap::iterator itr = m_messageMap.find(pMessage->command);
	if( itr != m_messageMap.end() )
	{
		IRCSessionMessageHandler cb = itr->second;
		(this->*cb)(*pMessage);
	} else {
		std::cout << "Unhandled IRC command: " << pMessage->command << std::endl;
	}
}

void IRCSession::Update()
{
	/*
	 * If we're at this point we haven't sent anything into the socket
	 * initialize the connection with NICK and USER.
	 */
	SendMessage(MessageFactory::NickName("Bwee"));
	SendMessage(MessageFactory::User("Bwee", "localhost", "localhost", "Bwee"));

	while(m_socket->isConnected() && !m_hasQuit)
	{
		while( m_socket->hasLine() && !m_hasQuit)
		{
			std::string msg = m_socket->readLine();
			Parse(msg);
		}
		Sleep(SESSION_UPDATE_RESOLUTION);
	}
}

void IRCSession::Quit(const char* pMessage)
{
	SendMessage(MessageFactory::Quit(pMessage));
	m_hasQuit = true;
}

void IRCSession::SendMessage(IRCMessage* pMessage)
{
	m_socket->sendLine(pMessage->toString());
	std::cout << pMessage->toString() << std::endl;
	delete pMessage;
}

void IRCSession::SendMessage(IRCMessage& pMessage)
{
	m_socket->sendLine(pMessage.toString());
}