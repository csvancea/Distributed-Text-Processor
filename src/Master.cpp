#include <mpi.h>
#include <thread>

#include "Logger.h"
#include "Master.h"


Master::Master(const std::string& inFile)
{
    size_t dotIdx = inFile.find_last_of('.');

    if (dotIdx == std::string::npos || inFile.substr(dotIdx) != ".in") {
        LOG_FATAL("Invalid extension for file name \"{}\"", inFile);
    }

    _baseFileName = inFile.substr(0, dotIdx);
}

Master::~Master()
{

}

void Master::Start()
{
    std::thread threads[MASTER_NUM_THREADS];

    for (int i = 0; i != MASTER_NUM_THREADS; ++i) {
        threads[i] = std::thread(&Master::WorkerThread, this, i+Node::TYPE_WORKER_HORROR);     
    }

    for (int i = 0; i != MASTER_NUM_THREADS; ++i) {
        threads[i].join();      
    }
}

void Master::WorkerThread(int nodeType)
{
    std::string paragraphName = GetParagraphNameFromNodeType(nodeType);
    if (paragraphName.empty()) {
        LOG_FATAL("Empty paragraph name for node type: {}", nodeType);
    }

    LOG_DEBUG("Started worker thread for paragraph name: {} (node type: {})", paragraphName, nodeType);

    ParseAndSendToWorkers(nodeType, paragraphName);
    ReceiveAndReassembleFromWorkers(nodeType, paragraphName);
}

void Master::ParseAndSendToWorkers(int nodeType, const std::string& paragraphName)
{
    enum eParserStates {
        WAITING_FOR_PARAGRAPH,
        SKIPPING_UNTIL_NEXT_PARAGRAPH,
        READING_PARAGRAPH   
    };

    int state = WAITING_FOR_PARAGRAPH;
    int paragraphIdx = -1;
    std::string line;
    std::ifstream inFile(_baseFileName + ".in");

    if (!inFile) {
        LOG_FATAL("\"{}\" paragraph handler couldn't open file: \"{}.in\"", paragraphName, _baseFileName);
    }

    while (std::getline(inFile, line)) {
        switch (state) {
        case WAITING_FOR_PARAGRAPH:
            paragraphIdx++;

            if (line == paragraphName) {
                state = READING_PARAGRAPH;
                
                LOG_DEBUG("Sending paragraph ID {} to worker node: \"{}\"", paragraphIdx, paragraphName);
                MPI_Send(&paragraphIdx, 1, MPI_INT, nodeType, 0, MPI_COMM_WORLD);
            } else {
                state = SKIPPING_UNTIL_NEXT_PARAGRAPH;
            }
            break;

        case SKIPPING_UNTIL_NEXT_PARAGRAPH:
            if (line.empty()) {
                state = WAITING_FOR_PARAGRAPH;
            }
            break;

        case READING_PARAGRAPH:
            LOG_DEBUG("Sending line \"{}\" of paragraph ID {} to worker node: \"{}\"", line, paragraphIdx, paragraphName);
            MPI_Send(line.c_str(), line.size() + 1, MPI_CHAR, nodeType, 0, MPI_COMM_WORLD);

            if (line.empty()) {
                // entire paragraph read!
                state = WAITING_FOR_PARAGRAPH;
            }
            break;
        }
    }

    if (state != WAITING_FOR_PARAGRAPH) {
        LOG_FATAL("Invalid input file content. Make sure it ends with an empty line. (last state: {}, last line parsed: \"{}\")", state, line);
    }
}

void Master::ReceiveAndReassembleFromWorkers(int nodeType, const std::string& paragraphName)
{
    // TODO
}

std::string Master::GetParagraphNameFromNodeType(int nodeType)
{
    switch (nodeType)
    {
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
