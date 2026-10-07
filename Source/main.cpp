#include <Core/Log.h>
#include <Application/Application.h>


int main() 
{
	KT::Application::Application app(1280, 720, "KT_Forge");
	app.Run();

	return 0;
}