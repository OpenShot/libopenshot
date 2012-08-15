#include "UnitTest++.h"
#include "../include/OpenShot.h"

using namespace std;
using namespace openshot;

// Since it is not possible to instantiate an abstract class, this test creates
// a new derived class, in order to test the base class file info struct.
TEST(FileReaderBase_Derived_Class)
{
	// Create a new derived class from type FileReaderBase
	class TestReader : public FileReaderBase
	{
	public:
		TestReader() { InitFileInfo(); };
		Frame* GetFrame(int number) { Frame *f = new Frame(); return f; }
	};

	// Create an instance of the derived class
	TestReader t1;

	// Check some of the default values of the FileInfo struct on the base class
	// If InitFileInfo() is not called in the derived class, these checks would fail.
	CHECK_EQUAL(false, t1.info.has_audio);
	CHECK_EQUAL(false, t1.info.has_audio);
	CHECK_CLOSE(0.0f, t1.info.duration, 0.00001);
	CHECK_EQUAL(0, t1.info.height);
	CHECK_EQUAL(0, t1.info.width);
	CHECK_EQUAL(1, t1.info.fps.num);
	CHECK_EQUAL(1, t1.info.fps.den);
}
