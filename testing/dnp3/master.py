import logging, sys, time

from pydnp3 import opendnp3, openpal, asiopal, asiodnp3
from visitors import *

FILTERS = opendnp3.levels.NOTHING
HOST = "127.0.0.1"
LOCAL = "0.0.0.0"
PORT = 20000

stdout_stream = logging.StreamHandler(sys.stdout)
stdout_stream.setFormatter(logging.Formatter('%(asctime)s\t%(name)s\t%(levelname)s\t%(message)s'))

_log = logging.getLogger(__name__)
_log.addHandler(stdout_stream)
_log.setLevel(logging.DEBUG)


class TestDNP3Master:
    def __init__(self):
        self.manager = asiodnp3.DNP3Manager(1, asiodnp3.ConsoleLogger().Create())

        self.retry = asiopal.ChannelRetry().Default()
        self.channel = self.manager.AddTCPClient(
            "tcpclient",
            FILTERS,
            self.retry,
            HOST,
            LOCAL,
            PORT,
            asiodnp3.PrintingChannelListener().Create(),
        )

        self.stack_config = asiodnp3.MasterStackConfig()
        self.stack_config.master.responseTimeout = openpal.TimeDuration().Seconds(2)
        self.stack_config.link.RemoteAddr = 1024

        self.master = self.channel.AddMaster(
            "master",
            asiodnp3.PrintingSOEHandler().Create(),
            asiodnp3.DefaultMasterApplication().Create(),
            self.stack_config,
        )

        self.scan = self.master.AddClassScan(
            opendnp3.ClassField().AllClasses(),
            openpal.TimeDuration().Minutes(10),
            opendnp3.TaskConfig().Default(),
        )

        self.master.Enable()
        time.sleep(5)

    def send_direct_operate_command(self, command, index, callback=asiodnp3.PrintingCommandCallback.Get()):
        self.master.DirectOperate(command, index, callback, opendnp3.TaskConfig().Default())

    def send_select_and_operate_command(self, command, index, callback=asiodnp3.PrintingCommandCallback.Get()):
        self.master.SelectAndOperate(command, index, callback, opendnp3.TaskConfig().Default())

    def shutdown(self):
        del self.scan
        del self.master
        del self.channel
        self.manager.Shutdown()


def collection_callback(result=None):
    print("Header: {0} | Index:  {1} | State:  {2} | Status: {3}".format(
        result.headerIndex,
        result.index,
        opendnp3.CommandPointStateToString(result.state),
        opendnp3.CommandStatusToString(result.status)
    ))


def command_callback(result=None):
    print("Received command result with summary: {}".format(opendnp3.TaskCompletionToString(result.summary)))
    result.ForeachItem(collection_callback)


def restart_callback(result=opendnp3.RestartOperationResult()):
    if result.summary == opendnp3.TaskCompletion.SUCCESS:
        print("Restart success | Restart Time: {}".format(result.restartTime.GetMilliseconds()))
    else:
        print("Restart fail | Failure: {}".format(opendnp3.TaskCompletionToString(result.summary)))


def main():
    app = TestDNP3Master()

    # ad-hoc tests can be performed at this point. See master_cmd.py for examples.
    app.send_direct_operate_command(
        opendnp3.ControlRelayOutputBlock(opendnp3.ControlCode.LATCH_OFF),
        10,
        command_callback,
    )

    time.sleep(10)
    app.scan.Demand()
    time.sleep(5)

    # uncomment the two lines below and watch all the output values from the
    # outstation get set to their zero value
    #app.master.Restart(opendnp3.RestartType.COLD, restart_callback)
    #time.sleep(5)

    app.shutdown()


if __name__ == '__main__':
    main()
