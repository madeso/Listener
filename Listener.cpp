#include "Listener.h"
#include "windows.h"

#ifdef LISTENER_USE_EXCEPTION
#define RETURNFALSE(reason)	throw ListenerError(reason)
#define RETURNTRUE	return
#else
#define RETURNFALSE(reason)	return false
#define RETURNTRUE	return true
#endif

#ifdef LISTENER_SUPPORT_SAPI
#include <sphelper.h>
#pragma warning(disable:4995) // Fixing problems with shelper.h
#endif

#define CONST_FOREACH(TYPE, VAR, CONT) for(TYPE::const_iterator VAR=CONT.begin(); VAR!=CONT.end();++VAR)

namespace listener
{
	std::wstring ConvertString(const std::string& string)
	{
		if ( string.empty() == true )
			return L"";

		const size_t numberOfCharacters = string.length() + 1;
		wchar_t* temp = new wchar_t[ numberOfCharacters ];

		::MultiByteToWideChar( CP_ACP, 0, string.c_str(), numberOfCharacters, temp, numberOfCharacters );

		const std::wstring ret = temp;
		delete temp;
		return ret;
	}

	std::string ConvertString(const std::wstring& string)
	{
		if ( string.empty() == true )
			return "";

		const size_t numberOfCharacters = string.length() + 1;
		char* temp = new char[ numberOfCharacters ];

		::WideCharToMultiByte( CP_ACP, 0, string.c_str(), numberOfCharacters, temp, numberOfCharacters, NULL, NULL );

		const std::string ret = temp;
		delete temp;
		return ret;
	}

#ifdef LISTENER_USE_WIDESTRING
	STRING FromString(const std::string& s)
	{
		return ConvertString(s);
	}

	STRING FromWideString(const std::wstring& s)
	{
		return s;
	}

	std::string ToString(const STRING& s)
	{
		return ConvertString(s);
	}

	std::wstring ToWideString(const STRING& s)
	{
		return s;
	}
#else
	STRING FromString(const std::string& s)
	{
		return s;
	}

	STRING FromWideString(const std::wstring& s)
	{
		return ConvertString(s);
	}

	std::string ToString(const STRING& s)
	{
		return s;
	}

	std::wstring ToWideString(const STRING& s)
	{
		return ConvertString(s);
	}
#endif
}

// setup complete, let's get it on

namespace listener
{
#ifdef LISTENER_USE_EXCEPTION
	ListenerError::ListenerError(const std::string& ss)
		: s(ss)
	{
	}

	const char* ListenerError::what() const throw()
	{
		return s.c_str();
	}
#endif

	Phrase::Phrase(const STRING& phrase)
		: string(phrase)
		, weight(1)
	{
	}
	Phrase& Phrase::setWeight(float w)
	{
		weight = w;
		return *this;
	}

	List::List()
	{
	}
	List::List(const Phrase& phrase)
	{
		phrases.push_back(phrase);
	}
	List& List::operator<<(const Phrase& phrase)
	{
		phrases.push_back(phrase);
		return *this;
	}
	List& List::operator<<(const STRING& phrase)
	{
		phrases.push_back(phrase);
		return *this;
	}

	Rule::Rule(const STRING& n)
		: name(n)
	{
	}
	Rule& Rule::operator()(const List& list)
	{
		lists.push_back(list);
		return *this;
	}

	Grammar::Grammar()
	{
	}
	Grammar::Grammar(const Rule& rule)
	{
		rules.push_back(rule);
	}

	Result::Result()
		: value("")
		, percentage(-1)
	{
	}
	Result::Result(const STRING& v, float p)
		: value(v)
		, percentage(p)
	{
	}

	Listener::~Listener(){}

	class NullListener : public Listener
	{
	public:
		Void update(Result* res)
		{
			*res = Result(LISTENER_STRING(""), 0.0f);
			RETURNTRUE;
		}

		Void pause()
		{
			RETURNTRUE;
		}

		Void resume()
		{
			RETURNTRUE;
		}

		bool canListen()
		{
			return false;
		}
	};

#ifdef LISTENER_SUPPORT_SAPI
	class SapiListener : public Listener
	{
	public:
		SapiListener()
		{
		}

		Void init(const Grammar& g)
		{
			CoInitialize(0);

			HRESULT hr = cpEngine.CoCreateInstance(CLSID_SpInprocRecognizer);
			if( FAILED(hr) )
			{
				RETURNFALSE("Failed to create speech recognition engine");
			}

			hr = SpGetDefaultTokenFromCategoryId(SPCAT_AUDIOIN, &cpAudioToken);
			if( FAILED(hr) )
			{
				RETURNFALSE("Failed to get the default audio input device");
			}

			hr = SpCreateDefaultObjectFromCategoryId(SPCAT_AUDIOIN, &cpAudio);
			if( FAILED(hr) )
			{
				RETURNFALSE("Failed to connect the Device");
			}

			hr = cpEngine->SetInput(cpAudio, TRUE);	
			if( FAILED(hr) )
			{
				RETURNFALSE("Failed to set input");
			}

			hr = cpEngine->CreateRecoContext(&cpRecoCtx);
			if( FAILED(hr) )
			{
				RETURNFALSE("Failed to create recognition context to recieve events");
			}

			hr = cpRecoCtx->SetNotifyWin32Event();
			if( FAILED(hr) )
			{
				RETURNFALSE("Failed to use win32 events");
			}

			hEvent = cpRecoCtx->GetNotifyEventHandle();

			// Set events we are interested in (speech recognised in grammar // Not recognised in grammar).
			ULONGLONG ullEvents = SPFEI(SPEI_RECOGNITION) | SPFEI(SPEI_FALSE_RECOGNITION);

			hr = cpRecoCtx->SetInterest(ullEvents, ullEvents);
			if( FAILED(hr) )
			{
				RETURNFALSE("Failed to set events to queue");
			}

			hr = cpRecoCtx->CreateGrammar(1, &cpGram);
			if( FAILED(hr) )
			{
				RETURNFALSE("Failed to create the grammar");
			}

			// resource: http://msdn.microsoft.com/en-us/library/ms717885%28v=vs.85%29.aspx

			CONST_FOREACH(Grammar::Rules, rule, g.rules)
			{
				SPSTATEHANDLE firstHlist;
				hr = cpGram->GetRule(ToWideString(rule->name).c_str(), 0, SPRAF_Dynamic | SPRAF_TopLevel | SPRAF_Active, TRUE, &firstHlist);
				if( FAILED(hr) )
				{
					RETURNFALSE("failed to create rule");
				}

				std::vector<SPSTATEHANDLE> hlists; // null terminated list of handles matching the lists in the rule
				const size_t rulesize = rule->lists.size();
				hlists.reserve(rulesize+1);
				
				//generate states
				for(size_t listindex=0;listindex<rulesize;++listindex)
				{
					SPSTATEHANDLE add=NULL;
					if(listindex==0)
					{
						add = firstHlist;
					}
					else
					{
						hr = cpGram->CreateNewState(firstHlist, &add);
						if( FAILED(hr) )
						{
							RETURNFALSE("Failed to create list");
						}
					}
					hlists.push_back(add);
				}
				hlists.push_back(NULL);

				for(size_t listindex=0;listindex<rulesize;++listindex)
				{
					SPSTATEHANDLE current = hlists[listindex];
					SPSTATEHANDLE next = hlists[listindex+1];

					CONST_FOREACH(List::Phrases, phrase, rule->lists[listindex].phrases)
					{
						hr = cpGram->AddWordTransition(current, next, ToWideString(phrase->string).c_str(), L" ", SPWT_LEXICAL, phrase->weight, NULL);
						if( FAILED(hr) )
						{
							RETURNFALSE("Failed to add phrase");
						}
					}
				}
			}
			hr = cpGram->Commit(0);
			if( FAILED(hr) )
			{
				RETURNFALSE("Failed to commit changes");
			}

			/*hr = cpGram->LoadCmdFromFile( L"plancommands.cfg", SPLO_DYNAMIC);
			if( FAILED(hr) )
			{
				RETURNFALSE("Failed to load commands");;
			}*/

			// Activate the grammar rules
			hr = cpGram->SetRuleState(0, 0, SPRS_ACTIVE);
			if( FAILED(hr) )
			{
				RETURNFALSE("Failed to set rulestate");
			}

			RETURNTRUE;
		}

		~SapiListener()
		{
			cpGram.Release();
			cpRecoCtx.Release();
			cpEngine.Release();
			CoUninitialize();
		}

		Void update(Result* res)
		{
			// Set to 0 so it wont interrupt other code.
			WaitForSingleObject(hEvent, 0);

			CSpEvent evt;
			if ( evt.GetFrom(cpRecoCtx) == S_OK )
			{
				if ( evt.eEventId == SPEI_FALSE_RECOGNITION )
				{
					*res = Result(LISTENER_STRING("<?>"), 1); // There was apparent speech without valid recognition
					RETURNTRUE;
				}
				else
				{
					ISpPhrase* pPhrase = evt.RecoResult();
					SPPHRASE* pParts = 0;
					HRESULT hr = pPhrase->GetPhrase(&pParts);
					if( FAILED(hr) )
					{
						RETURNFALSE("Failed to get phrase");
					}
					LPWSTR pwszText = 0;
					hr = pPhrase->GetText(SP_GETWHOLEPHRASE, SP_GETWHOLEPHRASE, FALSE, &pwszText, 0);
					if( FAILED(hr) )
					{
						RETURNFALSE("Failed to get text");
					}
					*res = Result(FromWideString(pwszText), (pParts->Rule.Confidence+1.0f)/2);
					
					CoTaskMemFree(pParts);
					CoTaskMemFree(pwszText);
					RETURNTRUE;
				}
			}

			*res = Result(LISTENER_STRING(""), 1);
			RETURNTRUE;
		}

		Void pause()
		{
			RETURNFALSE("not implemented");
		}

		Void resume()
		{
			RETURNFALSE("not implemed");
		}

		bool canListen()
		{
			return true;
		}
	private:
		HANDLE hEvent;
		
		CComPtr<ISpRecognizer> cpEngine;
		CComPtr<ISpRecoContext> cpRecoCtx;
		CComPtr<ISpRecoGrammar> cpGram;

		CComPtr<ISpObjectToken> cpAudioToken;
		CComPtr<ISpAudio> cpAudio;
	};
#endif

	Listener* CreateSapiListener(const Grammar& g)
	{
#ifdef LISTENER_SUPPORT_SAPI
		SapiListener* s = new SapiListener();
#ifndef LISTENER_USE_EXCEPTION
		const bool result = 
#endif
		s->init(g);

#ifndef LISTENER_USE_EXCEPTION
		if( result == false )
		{
			delete s;
			return 0;
		}
#endif

		return s;
#else
		return 0;
#endif
	}

	Listener* Create(CreateType ct, const Grammar& g)
	{
		if( ct == CREATE_SAPI || ct == CREATE_BESTFIT )
		{
			Listener* s = CreateSapiListener(g);
			if( s ) return s;
		}
		
		return new NullListener();
	}

	void Destroy(Listener* li)
	{
		delete li;
	}
}