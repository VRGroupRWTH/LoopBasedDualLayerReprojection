#include "timer.hpp"

bool Timer::create()
{
    glGenQueries(1, &this->begin_query);
    glGenQueries(1, &this->end_query);

    return true;
}

void Timer::destroy()
{
    glDeleteQueries(1, &this->begin_query);
    glDeleteQueries(1, &this->end_query);
}

void Timer::begin()
{
    glQueryCounter(this->begin_query, GL_TIMESTAMP);
}

void Timer::end()
{
    glQueryCounter(this->end_query, GL_TIMESTAMP);
}

bool Timer::get_time(double& time, TimerUnit unit)
{
    GLuint begin_avaliable = 0;
    GLuint end_avaliable = 0;

    glGetQueryObjectuiv(this->begin_query, GL_QUERY_RESULT_AVAILABLE, &begin_avaliable);
    glGetQueryObjectuiv(this->end_query, GL_QUERY_RESULT_AVAILABLE, &end_avaliable);

    if (begin_avaliable == GL_FALSE || end_avaliable == GL_FALSE)
    {
        return false;
    }

    GLuint64 begin_time = 0;
    GLuint64 end_time = 0;

    glGetQueryObjectui64v(this->begin_query, GL_QUERY_RESULT, &begin_time);
    glGetQueryObjectui64v(this->end_query, GL_QUERY_RESULT, &end_time);

    switch (unit)
    {
    case TIMER_UNIT_NANOSECONDS:
        time = (end_time - begin_time);
        break;
    case TIMER_UNIT_MILLISECONDS:
        time = (end_time - begin_time) / (1000.0 * 1000.0);
        break;
    default:
        time = 0.0;
        break;
    }

    return true;
}