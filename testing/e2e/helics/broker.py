import time

import helics as h

b = h.helicsCreateBroker("zmq", "", "-f 2 --ipv4")

if h.helicsBrokerIsConnected(b):
    print('HELICS broker started')

    try:
        time.sleep(3600)
    except Exception as ex:
        pass
else:
    print('HELICS broker failed to start')
