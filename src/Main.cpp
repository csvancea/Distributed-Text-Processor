#include <mpi.h>
#include <sys/types.h>
#include <unistd.h>

#include "Logger.h"
#include "Nodes.h"
#include "Master.h"
#include "Worker.h"

int main (int argc, char *argv[])
{
    int numtasks, rank, provided;
    auto& logger = Logger::GetInstance();
    Node* node = nullptr;
    std::string nodeName;

    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    nodeName = Node::GetNodeNameFromNodeType(rank);
    logger.SetOutputToFile(true, Logger::RULE_ALL, fmt::format("logs/node_{}.log", nodeName));
    logger.SetOutputToStdout(true);
    logger.SetID(nodeName);

    LOG_DEBUG("Started process ID: {}", getpid());

    switch (rank) {
    case Node::TYPE_MASTER:
        if (argc != 2) {
            LOG_FATAL("Invalid command line arguments specified (argc = {})", argc);
        }

        node = new Master(argv[1]);
        break;
    case Node::TYPE_WORKER_HORROR:
        node = new WorkerHorror();
        break;
    case Node::TYPE_WORKER_COMEDY:
        node = new WorkerComedy();
        break;
    case Node::TYPE_WORKER_FANTASY:
        node = new WorkerFantasy();
        break;
    case Node::TYPE_WORKER_SF:
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
