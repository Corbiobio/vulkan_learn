#include <string>
#include <fstream>
#include <sstream>

void read_file(const std::string& path, std::string& res)
{
	std::ifstream file {path};
	if (!file.is_open())
		return;

	std::stringstream fileContents;
	fileContents << file.rdbuf();
	res = fileContents.str();
	file.close();
}