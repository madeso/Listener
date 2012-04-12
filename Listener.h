#ifndef LISTENER_H_INCLUDED
#define LISTENER_H_INCLUDED

#include <string>
#include <vector>

#define LISTENER_SUPPORT_SAPI
#define LISTENER_USE_EXCEPTION
//#define LISTENER_USE_WIDESTRING

#ifdef LISTENER_USE_EXCEPTION
	typedef void Void;
#else
	typedef bool Void;
#endif

#ifdef LISTENER_USE_WIDESTRING
	typedef std::wstring STRING;
	#define LISTENER_STRING(x)	STRING(L##x)
#else
	typedef std::string STRING;
	#define LISTENER_STRING(x)	STRING(x)
#endif


namespace listener
{
#ifdef LISTENER_USE_EXCEPTION
	class ListenerError : public std::exception
	{
	public:
		ListenerError(const std::string& ss);
		const char* what() const throw();
	private:
		std::string s;
	};
#endif

	struct Phrase
	{
		Phrase(const STRING& phrase);
		STRING string;
		float weight;

		Phrase& setWeight(float w);
	};

	struct List
	{
		List();
		List(const Phrase& phrase);

		typedef std::vector<Phrase> Phrases;
		Phrases phrases;

		List& operator<<(const Phrase& phrase); /// syntactic sugar for adding phrases
		List& operator<<(const STRING& phrase); /// syntactic sugar for adding phrases
	};

	struct Rule
	{
		explicit Rule(const STRING& name);

		typedef std::vector<List> Lists;
		Lists lists;
		STRING name;

		Rule& operator()(const List& list); /// syntactic sugar for adding lists
	};

	struct Grammar
	{
		Grammar();
		Grammar(const Rule& rule); /// syntactic sugar for single rule grammars

		typedef std::vector<Rule> Rules;
		Rules rules;
	};

	struct Result
	{
		Result();
		Result(const STRING& v, float p);

		STRING value; /// the recognized value
		float percentage; /// 0-1 0=unsure, 1=100% sure
	};

	class Listener
	{
	public:
		virtual ~Listener();

		virtual Void update(Result* res) = 0;
		virtual Void pause() = 0;
		virtual Void resume() = 0;
		virtual bool canListen() = 0;
	};

	enum CreateType
	{
		CREATE_NULL /// listener that does nothing
		, CREATE_SAPI /// windows sapi listener
		, CREATE_BESTFIT // sapi for windows systems, null for other
	};

	Listener* Create(CreateType ct, const Grammar& g);
	void Destroy(Listener* li);
}

#endif
