#pragma once

#include <string>
#include <fstream>
#include "../types/transaction.h"
#include "../types/csv_line.h"

class CSVReader {
public:
    CSVReader(const std::string& filename);
    int readNextSet(types::TransactionSet& set);
    ~CSVReader();

private:
    std::string filename;
    std::ifstream file;
    int currentSetNo;
    types::TransactionSet currentTransactionSet;
    types::TransactionSet nextTransactionSet;
    
    void stripLineEndings(std::string& line);
    types::Transaction parseTransaction(const std::string& column);
    std::vector<std::string> parseServers(const std::string& column);
    types::CSVLine parseCSVLine(const std::string& line);
};