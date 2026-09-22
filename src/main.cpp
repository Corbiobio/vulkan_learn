#include "App.hpp"

int main(int argc, char** argv)
{
	App app;
	if (app.initialise())
		app.run();

	app.shutdown();
	return (0);
}