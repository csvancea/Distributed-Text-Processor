#include "Nodes.h"

std::string Node::GetNodeNameFromNodeType(int nodeType)
{
    switch (nodeType)
    {
        case Node::TYPE_MASTER:
            return "master";
        case Node::TYPE_WORKER_HORROR:
            return "horror";
        case Node::TYPE_WORKER_COMEDY:
            return "comedy";
        case Node::TYPE_WORKER_FANTASY:
            return "fantasy";
        case Node::TYPE_WORKER_SF:
            return "science-fiction";
    }
    return "";
}
