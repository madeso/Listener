#ifndef LISTENER_H_INCLUDED
#define LISTENER_H_INCLUDED

#include <string>

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
#else
	typedef std::string STRING;
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

	Listener* Create(CreateType ct);
	void Destroy(Listener* li);
}

#endif
