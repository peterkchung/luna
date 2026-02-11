// About: Entry point for the Luna lunar landing simulator.

#include "util/Log.h"

int main() {
    luna::util::Log::init();
    LOG_INFO("Luna starting");
    LOG_INFO("Luna shutting down");
    return 0;
}
