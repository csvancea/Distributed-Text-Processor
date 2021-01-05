#pragma once

#include <atomic>
#include <string>
#include <vector>

#include "Nodes.h"

#define MASTER_NUM_THREADS 4

class Master : public Node
{
public:
    Master(const std::string& inFile);

    virtual ~Master() override;
    virtual void Start() override;

private:
    void WorkerThread(int workerNode);
    void ParseAndSendToWorkerNode(int workerNode, const std::string& paragraphName);
    void ReceiveAndReassembleFromWorkerNode(int workerNode, const std::string& paragraphName);

    void WriteOutputFile();


    struct Paragraph
    {
        int paragraphType;
        std::string fullParagraph;
    };

    std::string _inFileName;
    std::string _outFileName;
    std::atomic<bool> _paragraphsListInitialized;
    std::vector<Master::Paragraph> _paragraphsList;
};
