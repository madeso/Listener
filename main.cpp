#include <iostream>
#include <memory>

#include "Listener.h"

#include "windows.h" // for IsDown below
bool IsDown(char c)
{
	const SHORT s = ::GetAsyncKeyState(c);
	int d = HIWORD(s);
	if( d ) return true;
	else return false;
}

void main()
{
	std::cout << "Launching listener..." << std::endl;

	try
	{
		std::auto_ptr<listener::Listener> li(listener::Create(listener::CREATE_BESTFIT));
		if( li->canListen() == false )
		{
			std::cerr << "Selected lestener is not listening, aborting..." << std::endl;
			std::cin.get();
			return;
		}

		std::cout << "Listener launched, press Q to quit..." << std::endl;

		while(true)
		{
			listener::Result res;
			li->update(&res);
			//if( res.percentage > 0.9 )
			{
				if( res.value != "" )
				{
					std::cout << "Detected: " << res.value << std::endl;
				}
			}
			
			if(IsDown('Q'))
			{
				return;
			}
		}
	}
	catch(const listener::ListenerError& le)
	{
		std::cerr << "Aborting because of error: " << le.what() << std::endl;
		std::cin.get();
	}
}