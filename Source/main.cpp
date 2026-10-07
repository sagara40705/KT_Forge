#include <Core/Log.h>
#include <Application/Application.h>

#include <stdexcept>
#include <execution>

int main() 
{
    try
    {
        KT::Application::Application app(1280, 720, "KT_Forge");
        app.Run();
    }
	catch (const std::exception& error)
	{
		KT_LOG_ERROR(error.what());
		return 1;
	}

	return 0;
}