Feature: command queue execution
    Background:
        word cmd() { return 1; }

    Scenario: simple queue iteration
        CommandQueue.add(cmd);
        CommandQueue.add(cmd);
        assert(0==_nextExecute);
        assert(2==_nextAdd);

        handle();
        assert(1==_nextExecute);
        assert(2==_nextAdd);
        delay(5);
        handle();
        assert(0==_nextAdd);
        assert(QUEUE_MAX_SIZE==_nextExecute);


    Scenario: command dropping
        for (1..19) CommandQueue.add(cmd);
        assert(0==_nextExecute);
        assert(19==_nextAdd);
        CommandQueue.add(cmd);
        assert(0==_nextExecute);
        assert(0==_nextAdd);
        assert( !CommandQueue.add(cmd) ); // to be dropped


    Scenario: queue overflow
        for (1..20) CommandQueue.add(cmd);
        assert(0==_nextExecute);
        assert(0==_nextAdd);
        handle();
        assert(1==_nextExecute);
        assert(0==_nextAdd);
        assert( CommandQueue.add(cmd) );



