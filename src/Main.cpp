#include <mpi.h>
#include <sys/types.h>
#include <unistd.h>

#include "Logger.h"
#include "Nodes.h"
#include "Master.h"
#include "Worker.h"

int main (int argc, char *argv[])
{
    int numtasks, rank, provided = -1;
    auto& logger = Logger::GetInstance();
    Node* node = nullptr;
    std::string nodeName;

    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    nodeName = Node::GetNodeNameFromRank(rank);
    logger.SetID(nodeName);
    logger.SetOutputToStdout(true);
#ifdef ENABLE_LOGGING
    logger.SetOutputToFile(true, Logger::RULE_ALL, fmt::format("logs/node_{}.log", nodeName));
#endif

    LOG_DEBUG("Started process ID: {}", getpid());

    if (provided != MPI_THREAD_MULTIPLE) {
        LOG_FATAL("MPI_THREAD_MULTIPLE is not supported (provided = {})", provided);
    }

    switch (rank) {
    case Node::RANK_MASTER:
        if (argc != 2) {
            LOG_FATAL("Invalid command line arguments specified (argc = {})", argc);
        }

        node = new Master(argv[1]);
        break;
    case Node::RANK_WORKER_HORROR:
        node = new WorkerHorror();
        break;
    case Node::RANK_WORKER_COMEDY:
        node = new WorkerComedy();
        break;
    case Node::RANK_WORKER_FANTASY:
        node = new WorkerFantasy();
        break;
    case Node::RANK_WORKER_SF:
        node = new WorkerSF();
        break;
    default:
        LOG_FATAL("Invalid number of tasks: {}, expected: {}", numtasks, Node::NUM_NODE_TYPES);
    }

    node->Start();
    delete node;

    MPI_Finalize();
    return 0;
}
