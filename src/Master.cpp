#include <mpi.h>
#include <thread>

#include "Logger.h"
#include "Master.h"


Master::Master(const std::string& inFile) : _paragraphsListInitialized(false)
{
    _inFileName = inFile;
    _outFileName = inFile + ".out";
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

    WriteOutputFile();
}

void Master::WorkerThread(int nodeType)
{
    std::string paragraphName = GetNodeNameFromNodeType(nodeType);
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
                
                // LOG_DEBUG("Sending paragraph ID {} to worker node: \"{}\"", paragraphIdx, paragraphName);
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
            int lineLength = line.length();

            // LOG_DEBUG("Sending line \"{}\" of paragraph ID {} to worker node: \"{}\"", line, paragraphIdx, paragraphName);
            
            MPI_Send(&lineLength, 1, MPI_INT, nodeType, 0, MPI_COMM_WORLD);
            if (lineLength) {
                MPI_Send(line.c_str(), lineLength, MPI_CHAR, nodeType, 0, MPI_COMM_WORLD);
            }
            else {
                // entire paragraph read!
                state = WAITING_FOR_PARAGRAPH;
            }
            break;
        }
    }

    if (state == READING_PARAGRAPH) {
        LOG_WARNING("Invalid input file content. Make sure it ends with an empty line. (last state: {}, last line parsed: \"{}\")", state, line);
        LOG_DEBUG("Sending an empty line to signal end of message");

        int lineLength = 0;
        MPI_Send(&lineLength, 1, MPI_INT, nodeType, 0, MPI_COMM_WORLD);
    }

    // this vector stores the content received from the worker nodes (paragraphs, same order as in input file)
    // only 1 out of the 4 threads must resize it (otherwise it may lead to corrupted data)
    // also, resizing the vector before sending the first FINISH message means that the modification will happen before any data is received from workers, so OOB writes are not possible
    bool expected = false;
    if (_paragraphsListInitialized.compare_exchange_strong(expected, true)) {
        _paragraphsList.resize(paragraphIdx + 1);
        LOG_DEBUG("Resized _paragraphsList to {} elements", paragraphIdx + 1);
    }

    // LOG_DEBUG("Sending FINISH message to node type: \"{}\"", paragraphName);
    paragraphIdx = -1;
    MPI_Send(&paragraphIdx, 1, MPI_INT, nodeType, 0, MPI_COMM_WORLD);
}

void Master::ReceiveAndReassembleFromWorkers(int nodeType, const std::string& paragraphName)
{
    LOG_DEBUG("Process incoming messages from worker node: {}", paragraphName);

    MPI_Status status;
    int commandOrParagraphId;
    int lineLength;

    while (1) {
        MPI_Recv(&commandOrParagraphId, 1, MPI_INT, nodeType, 0, MPI_COMM_WORLD, &status);
        if (commandOrParagraphId < 0) {
            // FINISH command
            break;
        }

        Paragraph& paragraph = _paragraphsList[commandOrParagraphId];
        paragraph.paragraphType = nodeType;

        while (1) {
            MPI_Recv(&lineLength, 1, MPI_INT, nodeType, 0, MPI_COMM_WORLD, &status);
            if (lineLength == 0) {
                break;
            }

            paragraph.lines.emplace_back(lineLength, ' ');
            MPI_Recv(&paragraph.lines.back()[0], lineLength, MPI_CHAR, nodeType, 0, MPI_COMM_WORLD, &status);
        }
    }
}

void Master::WriteOutputFile()
{
    std::ofstream outFile(_outFileName);

    if (!outFile) {
        LOG_FATAL("Couldn't open file: \"{}\"", _outFileName);
    }

    for (auto& paragraph : _paragraphsList) {
        outFile << GetNodeNameFromNodeType(paragraph.paragraphType) << std::endl;

        for (auto& line : paragraph.lines) {
            outFile << line << std::endl;
        }

        outFile << std::endl;
    }
}
