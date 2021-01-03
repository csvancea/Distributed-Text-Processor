#pragma once

#include <string>
#include "Nodes.h"

#define MASTER_NUM_THREADS 4

class Master : public Node
{
public:
    Master(const std::string& inFile);

    virtual ~Master() override;
    virtual void Start() override;

    void WorkerThread(int nodeType);
    void ParseAndSendToWorkers(int nodeType, const std::string& paragraphName);
    void ReceiveAndReassembleFromWorkers(int nodeType, const std::string& paragraphName);

    static std::string GetParagraphNameFromNodeType(int nodeType);

private:
    std::string _baseFileName;
};
