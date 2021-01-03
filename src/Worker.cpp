#include <mpi.h>
#include <thread>
#include <unistd.h>

#include "Logger.h"
#include "Worker.h"


Worker::Worker()
{

}

Worker::~Worker()
{

}

void Worker::Start()
{
    LOG_DEBUG("Worker started");

    _availableCores = sysconf(_SC_NPROCESSORS_CONF) - 1;
    if (_availableCores < 1) {
        LOG_FATAL("Expected at least 2 CPU cores, found: {}", _availableCores+1);
    }

    std::thread(&Worker::CommThread, this).join();
}

void Worker::CommThread()
{
    LOG_DEBUG("Comm thread started");

    CommReceive();
    CommSend();
}

void Worker::CommReceive()
{
    LOG_DEBUG("Process incoming messages");

    MPI_Status status;
    int commandOrParagraphId;

    while (1) {
        MPI_Recv(&commandOrParagraphId, 1, MPI_INT, TYPE_MASTER, 0, MPI_COMM_WORLD, &status);
        if (commandOrParagraphId < 0) {
            // FINISH command
            break;
        }

        ReceiveParagraph(commandOrParagraphId);
        ProcessLastParagraph();
    }
}

void Worker::CommSend()
{
    LOG_DEBUG("Process outgoing messages");

    int lineLength;

    for (auto& paragraph : _paragraphsList) {
        MPI_Send(&paragraph.globalIdx, 1, MPI_INT, TYPE_MASTER, 0, MPI_COMM_WORLD);

        for (auto& line : paragraph.lines) {
            lineLength = line.length();
            MPI_Send(&lineLength, 1, MPI_INT, TYPE_MASTER, 0, MPI_COMM_WORLD);
            MPI_Send(line.c_str(), lineLength, MPI_CHAR, TYPE_MASTER, 0, MPI_COMM_WORLD);
        }
        lineLength = 0;
        MPI_Send(&lineLength, 1, MPI_INT, TYPE_MASTER, 0, MPI_COMM_WORLD);
    }

    lineLength = -1;
    MPI_Send(&lineLength, 1, MPI_INT, TYPE_MASTER, 0, MPI_COMM_WORLD);
}

void Worker::ReceiveParagraph(int globalParagraphIdx)
{
    MPI_Status status;
    Paragraph paragraph;
    int lineLength;

    paragraph.globalIdx = globalParagraphIdx;

    while (1) {
        MPI_Recv(&lineLength, 1, MPI_INT, TYPE_MASTER, 0, MPI_COMM_WORLD, &status);
        if (!lineLength) {
            break;
        }

        paragraph.lines.emplace_back(lineLength, ' ');
        MPI_Recv(&paragraph.lines.back()[0], lineLength, MPI_CHAR, TYPE_MASTER, 0, MPI_COMM_WORLD, &status);
    }
    _paragraphsList.push_back(paragraph);
}

void Worker::ProcessLastParagraph()
{
    auto& paragraph = _paragraphsList.back();

    for (auto& line : paragraph.lines) {
        ProcessLine(line);
    }
}


void WorkerHorror::ProcessLine(std::string& line)
{
    line += __PRETTY_FUNCTION__;
}

void WorkerComedy::ProcessLine(std::string& line)
{
    line += __PRETTY_FUNCTION__;
}

void WorkerFantasy::ProcessLine(std::string& line)
{
    line += __PRETTY_FUNCTION__;
}

void WorkerSF::ProcessLine(std::string& line)
{
    line += __PRETTY_FUNCTION__;
}
