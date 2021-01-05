#pragma once

#include <string>


class Node
{
public:
    enum eNodeRank
    {
        RANK_MASTER,
        RANK_WORKER_HORROR,
        RANK_WORKER_COMEDY,
        RANK_WORKER_FANTASY,
        RANK_WORKER_SF,

        NUM_NODE_TYPES,
    };

    virtual ~Node() {};
    virtual void Start() = 0;

    static std::string GetNodeNameFromRank(int nodeType);
};

static_assert (Node::RANK_MASTER == Node::RANK_WORKER_HORROR-1, "eNodeRank" );
