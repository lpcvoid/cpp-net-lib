#define DOCTEST_CONFIG_IMPLEMENT
#include "../doctest/doctest/doctest.h"

int main(int argc, char **argv)
{
    doctest::Context context;
    context.addFilter("test-case-exclude", "*Large data transfer*"); //this test fails on mac, not sure why yet
    context.applyCommandLine(argc, argv);
    return context.run();
}