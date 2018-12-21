#include "Renderer.hpp"

int main(int argc, const char **args)
{
	Renderer renderer = Renderer(1080, 720);
	renderer.Run();
	return 0;
}
