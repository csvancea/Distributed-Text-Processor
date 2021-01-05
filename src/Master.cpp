#include <mpi.h>
#include <thread>

#include "Logger.h"
#include "Master.h"


Master::Master(const std::string& inFile) : _paragraphsListInitialized(false)
{
    size_t dotIdx = inFile.find_last_of('.');

    if (dotIdx == std::string::npos) {
        LOG_FATAL("Input file name has no extension (file: {})", inFile);
    }

    _inFileName = inFile;
    _outFileName = inFile.substr(0, dotIdx) + ".out";
}

Master::~Master()
{

}

void Master::Start()
{
    LOG_DEBUG("Master node started (inFile: \"{}\")", _inFileName);

    std::thread threads[MASTER_NUM_THREADS];

    for (int i = 0; i != MASTER_NUM_THREADS; ++i) {
        threads[i] = std::thread(&Master::WorkerThread, this, i+Node::TYPE_WORKER_HORROR);     
    }

    for (int i = 0; i != MASTER_NUM_THREADS; ++i) {
        threads[i].join();      
    }

    WriteOutputFile();
}

void Master::WorkerThread(int nodeType)
{
    std::string paragraphName = GetNodeNameFromNodeType(nodeType);
    if (paragraphName.empty()) {
        LOG_FATAL("Empty paragraph name for node type: {}", nodeType);
    }

    LOG_DEBUG("Worker thread started for paragraph name: {} (node type: {})", paragraphName, nodeType);

    LOG_DEBUG("Parsing and sending paragraphs to node: {}", paragraphName);
    ParseAndSendToWorkers(nodeType, paragraphName);

    LOG_DEBUG("Receiving and reassembling paragraphs from node: {}", paragraphName);
    ReceiveAndReassembleFromWorkers(nodeType, paragraphName);

    LOG_DEBUG("Worker thread ended for paragraph name: {}", paragraphName);
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
    std::string line, fullParagraph;
    std::ifstream inFile(_inFileName);

    if (!inFile) {
        LOG_FATAL("\"{}\" paragraph handler couldn't open file: \"{}\"", paragraphName, _inFileName);
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
            LOG_DEBUG("Read line \"{}\" of paragraph ID {} (\"{}\")", line, paragraphIdx, paragraphName);

            if (line.empty()) {
                // entire paragraph read!
                state = WAITING_FOR_PARAGRAPH;

                LOG_DEBUG("Sending paragraph ID {} to worker node: \"{}\" \"{}\"", paragraphIdx, paragraphName, fullParagraph);

                int paragraphLength = fullParagraph.length();
                MPI_Send(&paragraphLength, 1, MPI_INT, nodeType, 0, MPI_COMM_WORLD);
                MPI_Send(fullParagraph.c_str(), paragraphLength, MPI_CHAR, nodeType, 0, MPI_COMM_WORLD);

                fullParagraph.clear();
            }
            else {
                fullParagraph += line + '\n';
            }
            break;
        }
    }

    if (state == READING_PARAGRAPH) {
        LOG_WARNING("Invalid input file content. Make sure it ends with an empty line. (last state: {}, last line parsed: \"{}\")", state, line);
        LOG_DEBUG("Sending paragraph ID {} to worker node: \"{}\" \"{}\"", paragraphIdx, paragraphName, fullParagraph);

        int paragraphLength = fullParagraph.length();
        MPI_Send(&paragraphLength, 1, MPI_INT, nodeType, 0, MPI_COMM_WORLD);
        MPI_Send(fullParagraph.c_str(), paragraphLength, MPI_CHAR, nodeType, 0, MPI_COMM_WORLD);
    }

    // this vector stores the content received from the worker nodes (paragraphs, same order as in input file)
    // only 1 out of the 4 threads must resize it (otherwise it may lead to corrupted data)
    // also, resizing the vector before sending the first FINISH message means that the modification will happen before any data is received from workers, so OOB writes are not possible
    bool expected = false;
    if (_paragraphsListInitialized.compare_exchange_strong(expected, true)) {
        _paragraphsList.resize(paragraphIdx + 1);
        LOG_DEBUG("Resized _paragraphsList to {} elements", paragraphIdx + 1);
    }

    LOG_DEBUG("Sending FINISH message to node type: \"{}\"", paragraphName);
    paragraphIdx = -1;
    MPI_Send(&paragraphIdx, 1, MPI_INT, nodeType, 0, MPI_COMM_WORLD);
}

void Master::ReceiveAndReassembleFromWorkers(int nodeType, const std::string& paragraphName)
{
    LOG_DEBUG("Process incoming messages from worker node: {}", paragraphName);

    MPI_Status status;
    int commandOrParagraphId;
    int paragraphLength;

    while (1) {
        LOG_DEBUG("Waiting for command or paragraph from node: {} ...", paragraphName);
        MPI_Recv(&commandOrParagraphId, 1, MPI_INT, nodeType, 0, MPI_COMM_WORLD, &status);
        LOG_DEBUG("Received command or paragraph ID {} from node {}", commandOrParagraphId, paragraphName);
        if (commandOrParagraphId < 0) {
            // FINISH command
            LOG_DEBUG("Received FINISH({}) command from node {}", commandOrParagraphId, paragraphName);
            break;
        }

        LOG_DEBUG("Accessing paragraph ID {} from node {}", commandOrParagraphId, paragraphName);
        Paragraph& paragraph = _paragraphsList[commandOrParagraphId];
        paragraph.paragraphType = nodeType;

        LOG_DEBUG("Waiting for paragraph length of paragraph {} from node: {} ...", commandOrParagraphId, paragraphName);
        MPI_Recv(&paragraphLength, 1, MPI_INT, nodeType, 0, MPI_COMM_WORLD, &status);
        LOG_DEBUG("Waiting for paragraph {} of length {} from node: {} ...", commandOrParagraphId, paragraphLength, paragraphName);

        paragraph.fullParagraph.resize(paragraphLength);
        MPI_Recv(&paragraph.fullParagraph[0], paragraphLength, MPI_CHAR, nodeType, 0, MPI_COMM_WORLD, &status);
    }
}

void Master::WriteOutputFile()
{
    std::ofstream outFile(_outFileName);

    if (!outFile) {
        LOG_FATAL("Couldn't open file: \"{}\"", _outFileName);
    }

    for (auto& paragraph : _paragraphsList) {
        outFile << GetNodeNameFromNodeType(paragraph.paragraphType) << '\n';
        outFile << paragraph.fullParagraph;
    }
}
