#include "Nodes.h"

std::string Node::GetNodeNameFromRank(int nodeType)
{
    switch (nodeType)
    {
        case Node::RANK_MASTER:
            return "master";
        case Node::RANK_WORKER_HORROR:
            return "horror";
        case Node::RANK_WORKER_COMEDY:
            return "comedy";
        case Node::RANK_WORKER_FANTASY:
            return "fantasy";
        case Node::RANK_WORKER_SF:
            return "science-fiction";
    }
    return "";
}
