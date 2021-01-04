#include <mpi.h>
#include <thread>
#include <vector>
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

    int lineLength;

    for (auto& paragraph : _paragraphsList) {
        LOG_DEBUG("Sending paragraph ID {} to node: master ...", paragraph.globalIdx);
        MPI_Send(&paragraph.globalIdx, 1, MPI_INT, TYPE_MASTER, 0, MPI_COMM_WORLD);
        LOG_DEBUG("Sent paragraph ID {} to node: master", paragraph.globalIdx);

        for (auto& line : paragraph.lines) {
            lineLength = line.length();
            LOG_DEBUG("Sending line \"{}\" (length: {}) of paragraph ID {} to node: master ...", line, lineLength, paragraph.globalIdx);
            MPI_Send(&lineLength, 1, MPI_INT, TYPE_MASTER, 0, MPI_COMM_WORLD);
            MPI_Send(line.c_str(), lineLength, MPI_CHAR, TYPE_MASTER, 0, MPI_COMM_WORLD);
            LOG_DEBUG("Sent line \"{}\" (length: {}) of paragraph ID {} to node: master", line, lineLength, paragraph.globalIdx);
        }

        LOG_DEBUG("Sending end of paragraph for paragraph ID {} to node: master ...", paragraph.globalIdx);
        lineLength = 0;
        MPI_Send(&lineLength, 1, MPI_INT, TYPE_MASTER, 0, MPI_COMM_WORLD);

        LOG_DEBUG("Sent end of paragraph for paragraph ID {} to node: master", paragraph.globalIdx);
    }

    LOG_DEBUG("Sending of transmission to node: master ...");
    lineLength = -1;
    MPI_Send(&lineLength, 1, MPI_INT, TYPE_MASTER, 0, MPI_COMM_WORLD);
    LOG_DEBUG("Sent of transmission to node: master");
}

void Worker::ReceiveParagraph(int globalParagraphIdx)
{
    MPI_Status status;
    Paragraph paragraph;
    int lineLength;

    paragraph.globalIdx = globalParagraphIdx;

    while (1) {
        LOG_DEBUG("Receiving current line length for paragraph ID {}", globalParagraphIdx);
        MPI_Recv(&lineLength, 1, MPI_INT, TYPE_MASTER, 0, MPI_COMM_WORLD, &status);
        LOG_DEBUG("Received current line length for paragraph ID {} -> {}", globalParagraphIdx, lineLength);
        if (!lineLength) {
            break;
        }

        LOG_DEBUG("Receiving current line of length {} for paragraph ID {}", lineLength, globalParagraphIdx);
        paragraph.lines.emplace_back(lineLength, ' ');
        MPI_Recv(&paragraph.lines.back()[0], lineLength, MPI_CHAR, TYPE_MASTER, 0, MPI_COMM_WORLD, &status);
        LOG_DEBUG("Received current line (\"{}\") for paragraph ID {}", paragraph.lines.back(), globalParagraphIdx);
    }
    _paragraphsList.push_back(paragraph);
}

void Worker::ProcessLastParagraph()
{
    auto& paragraph = _paragraphsList.back();
    int numOfLines = paragraph.lines.size();
    int idx = 0;
    std::list<std::string>::iterator start_it, end_it, it = paragraph.lines.begin();

    LOG_DEBUG("Processing paragraph idx: {}", paragraph.globalIdx);

    while (it != paragraph.lines.end()) {
        if (idx % LINES_PER_WORKER_THREAD == 0) {
            start_it = it;
        }
        if (idx % LINES_PER_WORKER_THREAD == LINES_PER_WORKER_THREAD - 1 || idx == numOfLines - 1) {
            end_it = std::next(it);

            _threadPool.AddJob([=]() {
                for (auto i = start_it; i != end_it; ++i) {
                    ProcessLine(*i);
                }
            });
        }

        it++;
        idx++;
    }
}


void WorkerHorror::ProcessLine(std::string& line)
{
    std::string newLine;

    auto isConsonant = [](char ch) {
        int lowCh = tolower(ch);
        return (isalpha(ch) && lowCh != 'a' && lowCh != 'e' && lowCh != 'i' && lowCh != 'o' && lowCh != 'u');
    };

    for (auto ch : line) {
        newLine += ch;
        if (isConsonant(ch)) {
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

    auto split = [](const std::string& str, std::vector<std::string>& cont, char delim = ' ') {
        size_t current, previous = 0;
        current = str.find(delim);
        while (current != std::string::npos) {
            cont.push_back(str.substr(previous, current - previous));
            previous = current + 1;
            current = str.find(delim, previous);
        }
        cont.push_back(str.substr(previous, current - previous));
    };

    auto reverse = [](std::string& str) {
        size_t n = str.length();
    
        for (size_t i = 0; i < n/2; ++i) {
            std::swap(str[i], str[n-i-1]);
        }
    };


    split(line, tokens);
    for (size_t i = 6; i < tokens.size(); i += 7) {
        reverse(tokens[i]);
    }

    line.clear();
    for (auto& token : tokens) {
        line += token + ' ';
    }
    line.pop_back();
}
