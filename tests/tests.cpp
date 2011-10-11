#include <iostream>
#include "UnitTest++.h"

using namespace std;
using namespace UnitTest;

int main()
{
	cout << "----------------------------" << endl;
	cout << "     RUNNING ALL TESTS" << endl;
	cout << "----------------------------" << endl;

	// Run all unit tests
	RunAllTests();

	cout << "----------------------------" << endl;

	return 0;
}
