#include "emulator.h"
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

const bool debug = false;
int totalErrors = 0;

string intToHex(int number) {
  ostringstream ss;
  ss << hex << number;
  return ss.str();
}

int lenOfNum(int num) {
  return (num == 0) ? 1 : std::log10(std::abs(num)) + 1;
  ;
}

class Error {
public:
  string eType;
  string line;
  string eMessage;
  bool hasErrorMessage;
  int eCharLocation;
  int len;
  int lnNum;

  Error(string type, string ln, int location, int length, int lineNumber,
        string message) {
    hasErrorMessage = true;
    eType = type;
    line = ln;
    eMessage = message;
    eCharLocation = location;
    len = length;
    lnNum = lineNumber;
  }
  Error(string type, string ln, int location, int length, int lineNumber) {
    hasErrorMessage = false;
    eType = type;
    line = ln;
    eCharLocation = location;
    len = length;
    lnNum = lineNumber;
  }

  void print() {
    cout << "\033[31m" << eType << ": " << eMessage << "\033[0m" << endl;
    cout << lnNum << " " << line << endl;
    cout << string(lenOfNum(lnNum) + eCharLocation, '-') << string(len, '^')
         << endl;
    cout << "\033[31mError.\033[0m" << endl;
  }
};

// Log an error
void error(Error err) {
  totalErrors++;
  err.print();
}

// Safely parse argument tokens into 16-bit values
int parseToken(const string &tok, uint16_t &outVal) {
  try {
    size_t pos = 0;
    long long val = 0;

    // Hex prefix: 0x... or 0X...
    if (tok.size() > 2 && tok[0] == '0' && (tok[1] == 'x' || tok[1] == 'X')) {
      val = std::stoll(tok, &pos, 16);
    } else if (tok[0] == '/') {
      return 2;
    }
    // Explicit negative decimal number
    else if (tok[0] == '-') {
      val = std::stoll(tok, &pos, 10);
    }
    // Plain digits or hex strings
    else {
      // Check if string contains hex characters A-F/a-f
      bool isHex = false;
      for (char c : tok) {
        if ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
          isHex = true;
          break;
        }
      }
      val = std::stoll(tok, &pos, isHex ? 16 : 10);
    }

    if (pos != tok.size()) {
      return 1;
    }

    // Allow signed [-32768, 32767] or unsigned [0, 65535] range
    if (val < -32768 || val > 65535) {
      return 1;
    }

    outVal = static_cast<uint16_t>(val);
    return 0;
  } catch (...) {
    return 1;
  }
}

int assemble(string fName, DebugFlags flags) {
  string fileName = fName;

  ifstream SourceFile(fileName);
  if (!SourceFile.is_open()) {
    cout << "\033[31mFailed to open file.\033[0m" << endl;
    return 1;
  }

  int position = 0;
  int expectedArguments = 0;
  int lineNumber = 0;
  uint16_t immediate = 0;
  string line;
  int parseFeedback;

  while (getline(SourceFile, line)) {
    lineNumber += 1;

    bool hasImmediate = false;
    int immediatePosition = -1;
    uint16_t result = 0x0000;

    if (line.empty() ||
        (line.size() >= 2 && line[0] == '/' && line[1] == '/')) {
      continue;
    }

    stringstream ss(line);
    string word;
    vector<string> words;

    while (ss >> word) {
      words.push_back(word);
    }

    if (words.empty()) {
      continue;
    }

    string opcode = words[0];
    vector<uint16_t> args;
    bool argError = false;

    // Parse instruction arguments safely
    for (size_t i = 1; i < words.size(); ++i) {
      uint16_t parsedVal = 0;
      parseFeedback = parseToken(words[i], parsedVal);
      if (parseFeedback == 1) {
        error(Error("Argument Error", line, line.find(words[i]),
                    words[i].length(), lineNumber));
        cout << "Invalid argument \"" << words[i] << "\" on line " << lineNumber
             << endl;
        argError = true;
        break;
      } else if (parseFeedback == 2) {
        break;
      }

      if (debug) {
        cout << words[i] << endl;
      }
      args.push_back(parsedVal);
    }

    if (argError) {
      continue;
    }

    if (opcode == "AND") {
      result |= 0xF000;
      expectedArguments = 3;
    } else if (opcode == "OR") {
      result |= 0x1000;
      expectedArguments = 3;
    } else if (opcode == "NOT") {
      result |= 0x2000;
      expectedArguments = 2;
    } else if (opcode == "XOR") {
      result |= 0x3000;
      expectedArguments = 3;
    } else if (opcode == "ADD") {
      result |= 0x4000;
      expectedArguments = 3;
    } else if (opcode == "SUB") {
      result |= 0x5000;
      expectedArguments = 3;
    } else if (opcode == "SFL") {
      result |= 0x6000;
      expectedArguments = 3;
    } else if (opcode == "SFR") {
      result |= 0x7000;
      expectedArguments = 3;
    } else if (opcode == "LDR") {
      result |= 0x8000;
      expectedArguments = 2;
    } else if (opcode == "LDI") {
      result |= 0x9000;
      hasImmediate = true;
      immediatePosition = 1;
      expectedArguments = 2;
    } else if (opcode == "STR") {
      result |= 0xA000;
      expectedArguments = 2;
    } else if (opcode == "STI") {
      result |= 0xB000;
      hasImmediate = true;
      immediatePosition = 1;
      expectedArguments = 2;
    } else if (opcode == "JMP") {
      result |= 0xC000;
      hasImmediate = true;
      immediatePosition = 0;
      expectedArguments = 1;
    } else if (opcode == "JMZ") {
      result |= 0xD000;
      hasImmediate = true;
      immediatePosition = 0;
      expectedArguments = 1;
    } else if (opcode == "JMN") {
      result |= 0xE000;
      hasImmediate = true;
      immediatePosition = 0;
      expectedArguments = 1;
    } else if (opcode == "HLT") {
      result = 0;
      expectedArguments = 0;
      setRAM(position, result);
      break;
    } else {
      if (opcode == ".fill") {
        if (2 != static_cast<int>(args.size())) {
          error(Error("Argument Error", line, 5, line.length(), lineNumber,
                      "Incorrect number of arguments given"));
          cout << "Incorrect amount of arguments on line " << lineNumber
               << endl;
          continue;
        }

        // Check for invalid negative address
        if (args[0] > 0x7FFF) { // If parsed from negative, e.g. -2 -> 0xFFFE
          error(Error("Address Error", line, line.find(args[0]),
                      lenOfNum(args[0]), lineNumber));
          cout << "Invalid RAM address " << words[1] << " on line "
               << lineNumber << endl;
          continue;
        }

        setRAM(args[0], args[1]);
        if (debug) {
          cout << "RAM entry added at " << args[0] << " with value of "
               << static_cast<int16_t>(args[1]) << endl;
        }
      }
      continue;
    }

    if (expectedArguments > static_cast<int>(args.size())) {
      error(Error("Argument Error", line, 5, line.length(), lineNumber,
                  "Incorrect number of arguments given"));
      cout << "Incorrect amount of arguments on line " << lineNumber
           << ". Given " << args.size() << ", expected " << expectedArguments
           << endl;
      continue;
    } else if (expectedArguments != static_cast<int>(args.size())) {
      cout << "comments" << endl;
    }

    if (debug) {
      cout << opcode << endl;
      for (size_t i = 0; i < args.size(); i++) {
        cout << i;
      }
      cout << endl;
    }

    // Shift and merge register parameters into the instruction word
    for (size_t i = 0; i < expectedArguments; i++) {
      if (!hasImmediate || immediatePosition != static_cast<int>(i)) {
        uint16_t merge = args[i] << ((2 - i) * 4);
        result |= merge;
        if (debug) {
          cout << "Above are args" << endl;
        }
      }
    }

    // Write instruction word and immediate parameter to RAM
    if (hasImmediate) {
      setRAM(position, result);
      position++;
      setRAM(position, args[immediatePosition]);
      if (debug) {
        immediate = args[immediatePosition];
      }
    } else {
      setRAM(position, result);
    }

    if (debug) {
      cout << "RAM chunk for " << opcode << " on line " << lineNumber
           << " at position 0x" << intToHex(position) << "\n0x"
           << intToHex(result) << endl;
      if (hasImmediate) {
        cout << "0x" << intToHex(immediate) << endl;
      }
    }

    position++;
  }

  if (totalErrors == 0) {
    DebugFlags flags;
    flags.debug = true;
    flags.debugRegisters = true;
    flags.debugNonZeroRegisters = true;

    runProgram(flags);
  } else {
    cout << "\033[31m" << totalErrors << " generated, aborting program.\033[0m"
         << endl;
  }
  return 0;
}
