#include <mpi.h>

#include "Logger.h"
#include "Nodes.h"
#include "Master.h"

int main (int argc, char *argv[])
{
    int numtasks, rank, provided;
    auto& logger = Logger::GetInstance();
    Node* node = nullptr;

    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    logger.SetOutputToFile(true, Logger::RULE_ALL, fmt::format("logs/node_{}.log", rank));
    logger.SetOutputToStdout(true);

    if (numtasks != Node::NUM_NODE_TYPES) {
        LOG_FATAL("Invalid number of tasks: {}, expected: {}", numtasks, Node::NUM_NODE_TYPES);
    }

    switch (rank) {
    case Node::TYPE_MASTER:
        logger.SetID("master");

        if (argc != 2) {
            LOG_FATAL("Invalid command line arguments specified (argc = {})", argc);
        }

        node = new Master(argv[1]);
        break;
    case Node::TYPE_WORKER_HORROR:
        logger.SetID("horror");
        break;
    case Node::TYPE_WORKER_COMEDY:
        logger.SetID("comedy");
        break;
    case Node::TYPE_WORKER_FANTASY:
        logger.SetID("fantasy");
        break;
    case Node::TYPE_WORKER_SF:
        logger.SetID("sf");
        break;
    }

    if (node) {
        node->Start();
        delete node;
    }

    MPI_Finalize();
    return 0;
}
