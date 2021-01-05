#include <mpi.h>
#include <thread>
#include <vector>
#include <unistd.h>

#include "Logger.h"
#include "Worker.h"
#include "Utils.h"


Worker::Worker()
{

}

Worker::~Worker()
{

}

void Worker::Start()
{
    LOG_DEBUG("Worker node started");

    _availableCores = sysconf(_SC_NPROCESSORS_CONF);
    if (_availableCores < 2) {
        LOG_FATAL("Expected at least 2 CPU cores, found: {}", _availableCores);
    }

    std::thread(&Worker::CommThread, this).join();
}

void Worker::CommThread()
{
    LOG_DEBUG("Comm thread started");

    CommReceive();
    CommSend();

    LOG_DEBUG("Comm thread ended");
}

void Worker::CommReceive()
{
    LOG_DEBUG("Process incoming messages");

    MPI_Status status;
    int commandOrParagraphId;

    _threadPool.Start(_availableCores - 1);
    LOG_DEBUG("Started a thread pool of {} threads", _availableCores-1);

    while (1) {
        LOG_DEBUG("Waiting for command or paragraph from node: master ...");
        MPI_Recv(&commandOrParagraphId, 1, MPI_INT, TYPE_MASTER, 0, MPI_COMM_WORLD, &status);
        LOG_DEBUG("Received command or paragraph ID {} from node master", commandOrParagraphId);
        if (commandOrParagraphId < 0) {
            // FINISH command
            break;
        }

        LOG_DEBUG("Receiving paragraph ID {} from node master", commandOrParagraphId);
        ReceiveParagraph(commandOrParagraphId);

        LOG_DEBUG("Processing paragraph ID {} from node master", commandOrParagraphId);
        ProcessLastParagraph();

        LOG_DEBUG("Done processing paragraph ID {} from node master", commandOrParagraphId);
    }

    LOG_DEBUG("Waiting for jobs to complete ...");
    _threadPool.WaitForJobsToComplete();

    LOG_DEBUG("Shutting down thread pool ...");
    _threadPool.ShutDown();

    LOG_DEBUG("All incoming messages processed");
}

void Worker::CommSend()
{
    LOG_DEBUG("Process outgoing messages");

    std::string fullParagraph;
    int paragraphLength;

    for (auto& paragraph : _paragraphsList) {
        LOG_DEBUG("Sending paragraph ID {} to node: master ...", paragraph.globalIdx);
        MPI_Send(&paragraph.globalIdx, 1, MPI_INT, TYPE_MASTER, 0, MPI_COMM_WORLD);
        LOG_DEBUG("Sent paragraph ID {} to node: master", paragraph.globalIdx);

        for (auto& line : paragraph.lines) {
            fullParagraph += line + '\n';
        }

        LOG_DEBUG("Sending paragraph ID content {} to node: master ...", paragraph.globalIdx);
        paragraphLength = fullParagraph.length();
        MPI_Send(&paragraphLength, 1, MPI_INT, TYPE_MASTER, 0, MPI_COMM_WORLD);
        MPI_Send(fullParagraph.c_str(), paragraphLength, MPI_CHAR, TYPE_MASTER, 0, MPI_COMM_WORLD);
        LOG_DEBUG("Sent paragraph ID content {} to node: master", paragraph.globalIdx);

        fullParagraph.clear();
    }

    LOG_DEBUG("Sending of transmission to node: master ...");
    paragraphLength = -1;
    MPI_Send(&paragraphLength, 1, MPI_INT, TYPE_MASTER, 0, MPI_COMM_WORLD);
    LOG_DEBUG("Sent of transmission to node: master");
}

void Worker::ReceiveParagraph(int globalParagraphIdx)
{
    MPI_Status status;
    Paragraph paragraph;
    std::string fullParagraph;
    int paragraphLength;

    paragraph.globalIdx = globalParagraphIdx;

    LOG_DEBUG("Receiving length for paragraph ID {}", globalParagraphIdx);
    MPI_Recv(&paragraphLength, 1, MPI_INT, TYPE_MASTER, 0, MPI_COMM_WORLD, &status);
    LOG_DEBUG("Received length for paragraph ID {} -> {}", globalParagraphIdx, paragraphLength);

    LOG_DEBUG("Receiving content of paragraph ID {}", globalParagraphIdx);
    fullParagraph.resize(paragraphLength);
    MPI_Recv(&fullParagraph[0], paragraphLength, MPI_CHAR, TYPE_MASTER, 0, MPI_COMM_WORLD, &status);
    LOG_DEBUG("Received paragraph ID {}: \"{}\"", globalParagraphIdx, fullParagraph);

    Utils::Split(fullParagraph, paragraph.lines, '\n');
    _paragraphsList.push_back(std::move(paragraph));
}

void Worker::ProcessLastParagraph()
{
    auto& paragraph = _paragraphsList.back();
    auto numOfLines = paragraph.lines.size();

    LOG_DEBUG("Processing paragraph idx: {}", paragraph.globalIdx);

    for (size_t start = 0; start < numOfLines; start += LINES_PER_WORKER_THREAD) {
        size_t end = std::min(start + LINES_PER_WORKER_THREAD, numOfLines);

        _threadPool.AddJob([this, start, end, &paragraph]() {
            for (auto i = start; i != end; ++i) {
                ProcessLine(paragraph.lines[i]);
            }
        });
    }
}


void WorkerHorror::ProcessLine(std::string& line)
{
    std::string newLine;

    for (auto ch : line) {
        newLine += ch;
        if (Utils::IsConsonant(ch)) {
            newLine += static_cast<char>(tolower(ch));
        }
    }

    line = std::move(newLine);
}

void WorkerComedy::ProcessLine(std::string& line)
{
    std::string newLine;
    int idx = 1;

    for (auto ch : line) {
        if (ch == ' ') {
            idx = 0;
        }
        else if (idx % 2 == 0 && isalpha(ch)) {
            ch = static_cast<char>(toupper(ch));
        }

        newLine += ch;
        idx++;
    }

    line = std::move(newLine);
}

void WorkerFantasy::ProcessLine(std::string& line)
{
    std::string newLine;
    bool upperNext = true;

    for (auto ch : line) {
        if (ch == ' ') {
            upperNext = true;
        }
        else if (upperNext) {
            upperNext = false;
            if (isalpha(ch)) {
                ch = static_cast<char>(toupper(ch));
            }
        }

        newLine += ch;
    }

    line = std::move(newLine);
}

// WARNING: This function assumes that words are separed by a **SINGLE** space
void WorkerSF::ProcessLine(std::string& line)
{
    std::vector<std::string> tokens;

    Utils::Split(line, tokens);
    for (size_t i = 6; i < tokens.size(); i += 7) {
        Utils::Reverse(tokens[i]);
    }

    line.clear();
    for (auto& token : tokens) {
        line += token + ' ';
    }
    line.pop_back();
}
