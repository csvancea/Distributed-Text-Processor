#pragma once

#include <string>
#include <list>
#include "Nodes.h"

class Worker : public Node
{
protected:
    Worker();
    virtual void ProcessLine(std::string& line) = 0;

public:
    virtual ~Worker() override;
    virtual void Start() override;

private:
    void CommThread();
    void CommReceive();
    void CommSend();

    void ReceiveParagraph(int globalParagraphIdx);
    void ProcessLastParagraph();


    std::list<Paragraph> _paragraphsList;
    int _availableCores;
};

class WorkerHorror : public Worker
{
public:
    virtual ~WorkerHorror() override {};

protected:
    virtual void ProcessLine(std::string& line) override;
};

class WorkerComedy : public Worker
{
public:
    virtual ~WorkerComedy() override {};

protected:
    virtual void ProcessLine(std::string& line) override;
};

class WorkerFantasy : public Worker
{
public:
    virtual ~WorkerFantasy() override {};

protected:
    virtual void ProcessLine(std::string& line) override;
};

class WorkerSF : public Worker
{
public:
    virtual ~WorkerSF() override {};

protected:
    virtual void ProcessLine(std::string& line) override;
};
