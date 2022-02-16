# helics-helper

HELICS utility helper functions and classes. An improved version of this code
will soon be included as part of the official `pyhelics` Python package.

### Author

Dheepak Krishnamurthy (NREL)

### Usage

Write your federate in a Python class, and describe your actions in functions.

```python
from helics_helper import HelicsFederate, GlobalPublication, Publication, DataType

class SenderFederate(HelicsFederate):
    # Set federate name here as "Sender"
    federate_name = "Sender"

    # Set start time here as 5 seconds
    start_time = 5

    # Set end time here as 8 seconds
    end_time = 8

    # Set list of publications to be registered with HELICS
    # Publications can be global or not
    # Publications must include a topic name and a type of the data being published
    # Publications can be strings, integers, doubles, complex numbers or a list of doubles
    publications = [
        GlobalPublication("topic_name1", DataType.string),
        Publication("topic_name2", DataType.string),
        Publication("topic_name3", DataType.int),
        Publication("topic_name4", DataType.double),
        Publication("topic_name5", DataType.complex),
        Publication("topic_name6", DataType.vector),
    ]

    def action_publications(self, data, current_time):
        """
        Action to be taken before publications are published
        """

        # This action_publications function is called after every step time (defaults to 1 second)
        # The data dictionary should be populated by the user
        # Once populated, after this function returns, the data in the dictionary is published via helics.
        # If publications are defined above, this function must be implemented and the data dictionary must be populated

        time.sleep(1)

        # Sets `topic_name1` to "hello world at time {current_time}"
        data["topic_name1"] = "hello world at time {}".format(current_time)

        # Sets `Sender/topic_name2` to "goodbye world at time {current_time}"
        data["topic_name2"] = "goodbye world at time {}".format(current_time)

        # Data set to the various topics should match the publication DataType described above
        data["topic_name3"] = 7
        data["topic_name4"] = math.pi
        data["topic_name5"] = complex(1, 1)
        data["topic_name6"] = [1, 1.0, 2, 3.0, 5, 8.0, 13, 21.0] # In Python, list of doubles can be list of integers and floats

```