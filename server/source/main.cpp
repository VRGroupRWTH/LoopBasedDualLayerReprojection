#include "application.hpp"

int main(int argument_count, const char** argument_list)
{
    Application application;

    if (!application.create(argument_count, argument_list))
    {
        return -1;
    }

    if (!application.run())
    {
        return -1;
    }

    application.destroy();

    return 0;
}