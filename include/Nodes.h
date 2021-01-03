#pragma once

#include <list>
#include <string>


struct Paragraph
{
    union {
        int globalIdx;
        int paragraphType;
    };
    std::list<std::string> lines;
};

class Node
{
public:
    enum eNodeType
    {
        TYPE_MASTER,
        TYPE_WORKER_HORROR,
        TYPE_WORKER_COMEDY,
        TYPE_WORKER_FANTASY,
        TYPE_WORKER_SF,

        NUM_NODE_TYPES,
    };

    virtual ~Node() {};
    virtual void Start() = 0;

    static std::string GetNodeNameFromNodeType(int nodeType);
};

static_assert (Node::TYPE_MASTER == Node::TYPE_WORKER_HORROR-1, "eNodeType" );
