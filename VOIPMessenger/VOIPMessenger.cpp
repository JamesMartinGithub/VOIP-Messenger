// VOIPMessenger.cpp : Defines the entry point for the application.
//

#include "VOIPMessenger.h"
#include "Controller.h"

using namespace std;

int main(int argc, char* argv[]) {
	Controller* controller = new Controller();
	return controller->Start(argc, argv);
}
