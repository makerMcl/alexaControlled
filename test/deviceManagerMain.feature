Feature: main device switching
    Background:
        Given device A is on channel 1
        And   device B is on channel 3
        And   last active channel was 1
    Scenario: simple on
        Given the main device is off
        And device A is off
        And device B is off
        When device A is switched on
        And handle() is called multiple times for 30seconds
        Then the main device is on
        And device A is on
        And device B is off
        And and the active channel is 1

    Scenario: switching to 2nd device
        Given the main device is on
        And device A is on
        And device B is off
        When device B is switched on
        And handle() is called multiple times for 30seconds
        Then the main device is on
        And device A is off
        And device B is on
        And and the active channel is 3

    Scenario: switching to 2nd device while main device is starting
        Given the main device is off
        And device A is off
        And device B is off
        When device A is switched on
        And handle() is called multiple times for 3seconds
        And device B is switched on
        Then the main device is switching on
        And device A is off
        And device B is on
        And and the active channel is 3

