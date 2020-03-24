Feature: switching secondary device on and off
    Background:
        Given wait time is assumed as 30 seconds
    Scenario: simple on
        Given the device is off
        When state is set to on
        Then the device is starting up

    Scenario: simple off
        Given the device is on
        When state is set to off
        Then the device is shutting down

    Scenario: on while waiting for off
        Given the device is on
        When state is set to off
        And state is set to on
        Then the device is on

    Scenario: off while waiting for on
        Given the device is off
        When state is set to on
        And state is set to off
        Then the device is off

    Scenario: off while switching on
        Given the device is off
        When state is set to on
        And handle() is called
        And handle() is called 2sec later
        And the device is set to off
        And handle() is called
        And handle() is called 30sec later
        Then the device is off
        