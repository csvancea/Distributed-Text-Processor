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
    void WorkerThread(int nodeType);
    void ParseAndSendToWorkers(int nodeType, const std::string& paragraphName);
    void ReceiveAndReassembleFromWorkers(int nodeType, const std::string& paragraphName);

    void WriteOutputFile();


    std::string _baseFileName;
    std::atomic<bool> _paragraphsListInitialized;
    std::vector<Paragraph> _paragraphsList;
};
