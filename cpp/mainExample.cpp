#include <iostream>
#include <memory>
#include <chrono>
#include <thread>
#include "Utility/task.hpp"
#include "Utility/taskActions.hpp"

#include <conio.h> //nonstandard
using namespace MV;

int main() {
	std::cout << "Start\n";
	Task root("Root");
	char c = ' ';

	//Set up some tasks to show a few different ways to do so with a mix of ActionBase derived classes and custom behaviours.
	root.then("PrintCount", [](Task& t, double) {std::cout << t.elapsed() << "\n"; return t.elapsed() > 2.0f;}).recent()
		->localInterval(0.2f);

	root.then(std::make_shared<BlockForSeconds>(2.0)).recent()
		->onFinish.connect("Finish", [](Task&) {std::cout << "\n2 Second Invisible Wait Done... Press x to Quit\n"; });

	root.then(std::make_shared<BlockUntil>([&]() {return c == 'x'; })).recent()
		->onFinish.connect("Finish", [](Task&) {std::cout << "Goodbye!\n"; });

	//Execute the demo!
	auto start = std::chrono::high_resolution_clock::now();
	double timestep = 0.0f;
	while (!root.update(timestep)) {
		if (_kbhit()) { //nonstandard
			c = _getch();
		}
		auto end = std::chrono::high_resolution_clock::now();
		timestep = std::chrono::duration<double>(end - start).count();
		start = end;
	}
}