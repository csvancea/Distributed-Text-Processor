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

    while (1) {
        MPI_Recv(&commandOrParagraphId, 1, MPI_INT, TYPE_MASTER, 0, MPI_COMM_WORLD, &status);
        if (commandOrParagraphId < 0) {
            // FINISH command
            break;
        }

        ReceiveParagraph(commandOrParagraphId);
        ProcessLastParagraph();
    }

    _threadPool.WaitForJobsToComplete();
    _threadPool.ShutDown();
}

void Worker::CommSend()
{
    LOG_DEBUG("Process outgoing messages");

    std::string fullParagraph;
    int paragraphLength;

    for (auto& paragraph : _paragraphsList) {
        MPI_Send(&paragraph.globalIdx, 1, MPI_INT, TYPE_MASTER, 0, MPI_COMM_WORLD);

        for (auto& line : paragraph.lines) {
            fullParagraph += line + '\n';
        }

        paragraphLength = fullParagraph.length();
        MPI_Send(&paragraphLength, 1, MPI_INT, TYPE_MASTER, 0, MPI_COMM_WORLD);
        MPI_Send(fullParagraph.c_str(), paragraphLength, MPI_CHAR, TYPE_MASTER, 0, MPI_COMM_WORLD);

        fullParagraph.clear();
    }

    paragraphLength = -1;
    MPI_Send(&paragraphLength, 1, MPI_INT, TYPE_MASTER, 0, MPI_COMM_WORLD);
}

void Worker::ReceiveParagraph(int globalParagraphIdx)
{
    MPI_Status status;
    Paragraph paragraph;
    std::string fullParagraph;
    int paragraphLength;

    paragraph.globalIdx = globalParagraphIdx;

    MPI_Recv(&paragraphLength, 1, MPI_INT, TYPE_MASTER, 0, MPI_COMM_WORLD, &status);

    fullParagraph.resize(paragraphLength);
    MPI_Recv(&fullParagraph[0], paragraphLength, MPI_CHAR, TYPE_MASTER, 0, MPI_COMM_WORLD, &status);

    Utils::Split(fullParagraph, paragraph.lines, '\n');
    _paragraphsList.push_back(std::move(paragraph));
}

void Worker::ProcessLastParagraph()
{
    auto& paragraph = _paragraphsList.back();
    auto numOfLines = paragraph.lines.size();

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
