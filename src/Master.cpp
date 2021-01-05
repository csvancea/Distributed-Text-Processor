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
        threads[i] = std::thread(&Master::WorkerThread, this, i+Node::RANK_WORKER_HORROR);     
    }

    for (int i = 0; i != MASTER_NUM_THREADS; ++i) {
        threads[i].join();      
    }

    WriteOutputFile();
}

void Master::WorkerThread(int workerNode)
{
    std::string paragraphName = GetNodeNameFromRank(workerNode);
    if (paragraphName.empty()) {
        LOG_FATAL("Empty node name for worker node rank: {}", workerNode);
    }

    LOG_DEBUG("Master worker thread started for node: {}", paragraphName);

    ParseAndSendToWorkerNode(workerNode, paragraphName);
    ReceiveAndReassembleFromWorkerNode(workerNode, paragraphName);

    LOG_DEBUG("Master worker thread ended for node: {}", paragraphName);
}

void Master::ParseAndSendToWorkerNode(int workerNode, const std::string& paragraphName)
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

    LOG_DEBUG("Parsing and sending paragraphs to worker node: {}", paragraphName);

    if (!inFile) {
        LOG_FATAL("\"{}\" paragraph handler couldn't open file: \"{}\"", paragraphName, _inFileName);
    }

    while (std::getline(inFile, line)) {
        switch (state) {
        case WAITING_FOR_PARAGRAPH:
            paragraphIdx++;

            if (line == paragraphName) {
                state = READING_PARAGRAPH;
                MPI_Send(&paragraphIdx, 1, MPI_INT, workerNode, 0, MPI_COMM_WORLD);
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
            if (line.empty()) {
                // entire paragraph read!
                state = WAITING_FOR_PARAGRAPH;

                int paragraphLength = fullParagraph.length();
                MPI_Send(&paragraphLength, 1, MPI_INT, workerNode, 0, MPI_COMM_WORLD);
                MPI_Send(fullParagraph.c_str(), paragraphLength, MPI_CHAR, workerNode, 0, MPI_COMM_WORLD);

                fullParagraph.clear();
            }
            else {
                fullParagraph += line + '\n';
            }
            break;
        }
    }

    if (state == READING_PARAGRAPH) {
        LOG_DEBUG("Invalid input file ending. Make sure it ends with an empty line. (last state: {}, last line parsed: \"{}\", file: \"{}\")", state, line, _inFileName);

        int paragraphLength = fullParagraph.length();
        MPI_Send(&paragraphLength, 1, MPI_INT, workerNode, 0, MPI_COMM_WORLD);
        MPI_Send(fullParagraph.c_str(), paragraphLength, MPI_CHAR, workerNode, 0, MPI_COMM_WORLD);
    }

    // this vector stores the content received from the worker nodes (paragraphs, same order as in input file)
    // only 1 out of the 4 threads must resize it (otherwise it may lead to corrupted data)
    // also, resizing the vector before sending the first FINISH message means that the modification will happen before any data is received from workers, so OOB writes are not possible
    bool expected = false;
    if (_paragraphsListInitialized.compare_exchange_strong(expected, true)) {
        _paragraphsList.resize(paragraphIdx + 1);
        LOG_DEBUG("Resized _paragraphsList to {} elements", paragraphIdx + 1);
    }

    paragraphIdx = -1;
    MPI_Send(&paragraphIdx, 1, MPI_INT, workerNode, 0, MPI_COMM_WORLD);
}

void Master::ReceiveAndReassembleFromWorkerNode(int workerNode, const std::string& paragraphName)
{
    LOG_DEBUG("Process incoming messages from worker node: {}", paragraphName);
    (void)paragraphName; // silent unused warning when compiled with LOGGING disabled

    MPI_Status status;
    int commandOrParagraphId;
    int paragraphLength;

    while (1) {
        MPI_Recv(&commandOrParagraphId, 1, MPI_INT, workerNode, 0, MPI_COMM_WORLD, &status);
        if (commandOrParagraphId < 0) {
            // FINISH command
            break;
        }

        Master::Paragraph& paragraph = _paragraphsList[commandOrParagraphId];
        paragraph.paragraphType = workerNode;

        MPI_Recv(&paragraphLength, 1, MPI_INT, workerNode, 0, MPI_COMM_WORLD, &status);

        paragraph.fullParagraph.resize(paragraphLength);
        MPI_Recv(&paragraph.fullParagraph[0], paragraphLength, MPI_CHAR, workerNode, 0, MPI_COMM_WORLD, &status);
    }
}

void Master::WriteOutputFile()
{
    std::ofstream outFile(_outFileName);

    if (!outFile) {
        LOG_FATAL("Couldn't open file: \"{}\"", _outFileName);
    }

    for (auto& paragraph : _paragraphsList) {
        outFile << GetNodeNameFromRank(paragraph.paragraphType) << '\n';
        outFile << paragraph.fullParagraph;
    }
}
