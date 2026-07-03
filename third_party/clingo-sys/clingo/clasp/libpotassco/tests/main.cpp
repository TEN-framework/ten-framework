#if defined (_MSC_VER) && _MSC_VER >= 1200 && defined(ENABLE_DEBUG_HEAP)
#include <crtdbg.h>
int enableDebugHeap() {
	_CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) |
		_CRTDBG_LEAK_CHECK_DF | _CRTDBG_ALLOC_MEM_DF |
		_CRTDBG_CHECK_ALWAYS_DF);

	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
	return 1;
}
static int eh = enableDebugHeap();
#endif
#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include "catch.hpp"
