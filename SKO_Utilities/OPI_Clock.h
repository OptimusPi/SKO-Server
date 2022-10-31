#ifndef __OPI_CLOCK_H_
#define __OPI_CLOCK_H_

#include <chrono>

using namespace std::chrono;

class OPI_Clock
{
public:
    static unsigned long long int nanoseconds()
    {
        return time_point_cast<std::chrono::nanoseconds>(system_clock::now())
            .time_since_epoch().count();
    }
    static unsigned long long int microseconds()
    {
        return time_point_cast<std::chrono::microseconds>(system_clock::now())
            .time_since_epoch().count();
    }
    static unsigned long long int milliseconds()
    {
        return time_point_cast<std::chrono::milliseconds>(system_clock::now())
            .time_since_epoch().count();
    }
    static unsigned long long int seconds()
    {
        return time_point_cast<std::chrono::seconds>(system_clock::now())
            .time_since_epoch().count();
    }
};

#endif