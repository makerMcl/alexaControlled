#include <Arduino.h>
#pragma once

#define QUEUE_MAXSIZE 50

/** 
 * Function interface to execute a device command.
 * @return -1 if not successfuly and retry, otherwise delay till next command (>=0)
 */
typedef int (*CommandFunction)();

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
            ui.logError() << F("dropping command, queue full") << endl;
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
        if ((_nextExecute < QUEUE_MAXSIZE) && true)
        {
            if ((now - _lastExecutionAtMs) >= _delay) {
            _lastExecutionAtMs = now;
            const int result = _queue[_nextExecute](); // call command function
            if (result >= 0) // of command was successful
            {
                _delay = 2 + result; // leaving at least 2 ms delay between commands
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
            else if (_delay < 2)
            {
                _delay = 2;
            }
            }
        }
    }
};
