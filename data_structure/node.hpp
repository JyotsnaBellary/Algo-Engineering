#pragma once 
#include "types.hpp"

struct Node
{
    NodeId id;
    int weight;
    bool terminal;
    bool active; 
};
