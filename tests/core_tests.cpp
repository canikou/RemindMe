#include "remindme/parser.hpp"

#include <iostream>

namespace
{
bool testParseRelative()
{
    const remindme::ParseResult r = remindme::Parser::parseInput("Drink water in 45m");
    return r.ok && r.isRelative && r.title == "Drink water" && r.durationSeconds == 45 * 60;
}

bool testParseAtTime()
{
    const remindme::ParseResult r = remindme::Parser::parseInput("Stand up at 7:00AM");
    return r.ok && !r.isRelative && r.title == "Stand up" && r.timeOfDay.hour() == 7 && r.timeOfDay.minute() == 0;
}

bool testParseRepeat()
{
    const remindme::ParseResult r = remindme::Parser::parseInput("Hydrate in 10m every 2 hours");
    return r.ok && r.hasRepeatDirective && r.repeatIntervalSeconds == 2 * 3600;
}
}

int main()
{
    if (!testParseRelative())
    {
        std::cerr << "testParseRelative failed\n";
        return 1;
    }
    if (!testParseAtTime())
    {
        std::cerr << "testParseAtTime failed\n";
        return 1;
    }
    if (!testParseRepeat())
    {
        std::cerr << "testParseRepeat failed\n";
        return 1;
    }

    std::cout << "All tests passed.\n";
    return 0;
}
