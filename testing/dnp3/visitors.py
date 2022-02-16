"""
    The master uses these data-type-specific Visitor class definitions
    when it processes measurements received from the outstation.
"""
from pydnp3 import opendnp3


class VisitorIndexedBinary(opendnp3.IVisitorIndexedBinary):
    def __init__(self):
        super(VisitorIndexedBinary, self).__init__()
        self.index_and_value = []

    def OnValue(self, indexed_instance):
        self.index_and_value.append((indexed_instance.index, indexed_instance.value.value))


class VisitorIndexedDoubleBitBinary(opendnp3.IVisitorIndexedDoubleBitBinary):
    def __init__(self):
        super(VisitorIndexedDoubleBitBinary, self).__init__()
        self.index_and_value = []

    def OnValue(self, indexed_instance):
        self.index_and_value.append((indexed_instance.index, indexed_instance.value.value))


class VisitorIndexedCounter(opendnp3.IVisitorIndexedCounter):
    def __init__(self):
        super(VisitorIndexedCounter, self).__init__()
        self.index_and_value = []

    def OnValue(self, indexed_instance):
        self.index_and_value.append((indexed_instance.index, indexed_instance.value.value))


class VisitorIndexedFrozenCounter(opendnp3.IVisitorIndexedFrozenCounter):
    def __init__(self):
        super(VisitorIndexedFrozenCounter, self).__init__()
        self.index_and_value = []

    def OnValue(self, indexed_instance):
        self.index_and_value.append((indexed_instance.index, indexed_instance.value.value))


class VisitorIndexedAnalog(opendnp3.IVisitorIndexedAnalog):
    def __init__(self):
        super(VisitorIndexedAnalog, self).__init__()
        self.index_and_value = []

    def OnValue(self, indexed_instance):
        self.index_and_value.append((indexed_instance.index, indexed_instance.value.value))


class VisitorIndexedBinaryOutputStatus(opendnp3.IVisitorIndexedBinaryOutputStatus):
    def __init__(self):
        super(VisitorIndexedBinaryOutputStatus, self).__init__()
        self.index_and_value = []

    def OnValue(self, indexed_instance):
        self.index_and_value.append((indexed_instance.index, indexed_instance.value.value))


class VisitorIndexedAnalogOutputStatus(opendnp3.IVisitorIndexedAnalogOutputStatus):
    def __init__(self):
        super(VisitorIndexedAnalogOutputStatus, self).__init__()
        self.index_and_value = []

    def OnValue(self, indexed_instance):
        self.index_and_value.append((indexed_instance.index, indexed_instance.value.value))


class VisitorIndexedTimeAndInterval(opendnp3.IVisitorIndexedTimeAndInterval):
    def __init__(self):
        super(VisitorIndexedTimeAndInterval, self).__init__()
        self.index_and_value = []

    def OnValue(self, indexed_instance):
        # The TimeAndInterval class is a special case, because it doesn't have a "value" per se.
        ti_instance = indexed_instance.value
        ti_dnptime = ti_instance.time
        ti_interval = ti_instance.interval
        self.index_and_value.append((indexed_instance.index, (ti_dnptime.value, ti_interval)))
