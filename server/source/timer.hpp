#ifndef HEADER_TIMER
#define HEADER_TIMER

#include <GL/glew.h>
#include <vector>
#include <optional>

enum TimerUnit
{
    TIMER_UNIT_NANOSECONDS,
    TIMER_UNIT_MILLISECONDS
};

class Timer
{
private:
    GLuint begin_query = 0;
    GLuint end_query = 0;

public:
    Timer() = default;
    
    bool create();
    void destroy();

    void begin();
    void end();

    bool get_time(double& time, TimerUnit unit);
};

#endif