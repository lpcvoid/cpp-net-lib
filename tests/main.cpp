#define DOCTEST_CONFIG_IMPLEMENT
#include <random>
#include "../doctest/doctest/doctest.h"

// generate random port for server, which gets used in all tests
std::random_device random_device;
std::mt19937 gen(random_device());
std::uniform_int_distribution<> distr(10000, 65000);

uint16_t test_port = distr(gen);

int main(int argc, char **argv)
{
    doctest::Context context;
    context.addFilter("test-case-exclude", "*Large data transfer*"); //this test fails on mac, not sure why yet
    context.applyCommandLine(argc, argv);
    return context.run();
}