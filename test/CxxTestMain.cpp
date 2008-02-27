#include "tut.h"
#include "tut_reporter.h"
#include <apr_general.h>
#include <signal.h>
#include <cstdlib>

namespace tut {
	test_runner_singleton runner;
}

int main() {
	apr_initialize();
	tut::reporter reporter;
	tut::runner.get().set_callback(&reporter);
	signal(SIGPIPE, SIG_IGN);
	setenv("RAILS_ENV", "production", 1);
	try {
		tut::runner.get().run_tests();
	} catch (const std::exception &ex) {
		std::cerr << "Exception raised: " << ex.what() << std::endl;
		return 1;
	}
	return 0;
}
