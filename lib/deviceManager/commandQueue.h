#include <Arduino.h>
#pragma once

#define QUEUE_MAXSIZE 20

typedef unsigned int (*CommandFunction)();

/** 
 * Queue of function calls to be executed, in specified order and with timing.
 * Implements a circular ring buffer.
 */
class CommandQueue
{
private:
    CommandFunction _queue[QUEUE_MAXSIZE];
    byte _nextExecute = QUEUE_MAXSIZE; // === nothing to execute
    byte _nextAdd = 0;
    word _lastExecutionAtMs = 0;
    word _delay = 0;
    static byte nextPos(byte p) { return ++p >= QUEUE_MAXSIZE ? 0 : p; }

public:
    bool isEmpty()
    {
        return QUEUE_MAXSIZE == _nextExecute && 0 == _nextAdd;
    }

    bool addCommand(const CommandFunction cmd)
    {
        if (_nextAdd == _nextExecute)
        {
            // reached capacity of buffer, dropping this command
            return false;
        }
        else
        {
            _queue[_nextAdd] = cmd;
            if (_nextExecute >= QUEUE_MAXSIZE)
            {
                _nextExecute = _nextAdd;
            }
            _nextAdd = nextPos(_nextAdd);
            return true;
        }
    }

    void handle()
    {
        const unsigned long now = millis();
        if ((_nextExecute < QUEUE_MAXSIZE) && (now - _lastExecutionAtMs) >= _delay)
        {
            _lastExecutionAtMs = now;
            _delay = 2;                          // blocking command execution, but there should be no concurrency
            _delay = 2 + _queue[_nextExecute](); // call command function, leaving at least 2 ms delay between commands
            _queue[_nextExecute] = nullptr;
            _nextExecute = nextPos(_nextExecute);
            if (_nextExecute == _nextAdd)
            {
                // reset queue state if last command was executed
                _nextAdd = 0;
                _nextExecute = QUEUE_MAXSIZE;
                // Note: not resetting _nextActionAtMs, so newly added commands will respect pause of last command
            }
        }
    }
};
