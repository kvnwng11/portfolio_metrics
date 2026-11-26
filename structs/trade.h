#include <string>

#pragma once

struct Trade
{
    std::string asset;
    double price;
    double quantity;
    double timestamp;
};