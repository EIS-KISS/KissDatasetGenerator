#include <iostream>
#include <vector>
#include <memory>

int main(int argc, char** argv)
{
	std::vector<std::unique_ptr<int>> ids;
	std::unique_ptr<int> anInt(new int(0));
	ids.push_back(std::move(anInt));
	return 1;
}
