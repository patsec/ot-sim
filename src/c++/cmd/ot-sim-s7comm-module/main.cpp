#include <condition_variable>
#include <csignal>
#include <iostream>
#include <mutex>
#include <thread>

#include <boost/foreach.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include "fmt/format.h"

#include "msgbus/pusher.hpp"
#include "msgbus/subscriber.hpp"

#include "snap7/snap7_libmain.h"

//for multithreading, mutex
std::condition_variable cv;
std::mutex m;
namespace pt = boost::property_tree;

typedef std::shared_ptr<otsim::msgbus::Pusher> Pusher;

void signalHandler(int) {
    //if threads waiting for *this, notify_one unblocks one of the waiting threads
    cv.notify_one();
}

/*
LISTENER CLASS TO CONTINUALLY PUBLISH MESSAGES TO THE MSGBUS <------------- similar to DNP3 module
The main difference between this channel listener and the dnp3 channel listener is that this one does not deal with threading
because snap7 does not implement channels or channel states, which dnp3 bases its threading on.
There is still a thread that is used to run the channel listener, but it does not have any mutex implementation, so it should just work
normally.
This class should accomplish all pushing and interface through zeromq via the msgbus pusher class
*/
class Listener {
public:
    static std::shared_ptr<Listener> Create(std::string name, otsim::msgbus::Pusher pusher) {
        return std::make_shared<Listener>(name, pusher);
    }

    Listener(std::string name, otsim::msgbus::Pusher pusher) : name(name), pusher(pusher) {
        thread = std::thread(std::bind(&Listener::Run, this));
    }

    ~Listener() {};

    //primary loop, which continually publishes on 5 second intervals
    void Run() {
        while (true) {
            {
                publish();
            }

        std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }

private:
    /*
    Every module in ot-sim should push EVERYTHING, but should only subscribe to certain tags.
    For that reason, this publish function does not implement any tag-checking/ descrimination; instead,
    it pushes all envelopes to the msgbus, where other modules can determine whether or not they are relevant
    */
    void publish() {
        std::string tag   = fmt::format("{}.connected", name);

        std::cout << fmt::format("[{}] setting connected", name) << std::endl;

        otsim::msgbus::Points points;
        points.push_back(otsim::msgbus::Point{tag});

        otsim::msgbus::Status contents = {.measurements = points};
        auto env = otsim::msgbus::NewEnvelope(name, contents);

        pusher.Push("RUNTIME", env);
    }

    std::string             name;
    otsim::msgbus::Pusher   pusher;

    std::thread thread;
};

int main(int argc, char** argv){
    //argv will contain the path to the XML config file, if argc is less than 2, it means no path was included and the module cannot run
    if (argc < 2) {
        std::cerr << "ERROR: missing path to XML config file" << std::endl;
        return 1;
    }

    //keep servers in scope so their threads don't terminate immediately.
    std::vector<std::shared_ptr<otsim::snap7::S7Object>> servers;         

    //keep clients in scope so their threads don't terminate immediately.
    std::vector<std::shared_ptr<otsim::snap7::S7Object>> clients;

    // Keep client channel listeners in scope so their threads don't terminate immediately.
    std::vector<std::shared_ptr<Listener>> listeners;

    //keep subscribers in scope so their threads don't terminate immediately.
    std::vector<std::shared_ptr<otsim::msgbus::Subscriber>> subscribers;

    //the xml file will be read and parsed into a ptree
    pt::ptree tree;
    pt::read_xml(argv[1], tree);

    //main loop for the module. loops through XML data
    BOOST_FOREACH(pt::ptree::value_type &v, tree) {

        //ensure this is the ot-sim XML
        if (v.first.compare("ot-sim") != 0) {
            std::cerr << "ERROR: missing root 'ot-sim' element in XML config" << std::endl;
            return 1;
        }

        std::string pubEndpoint;
        std::string pullEndpoint;

        //v.second will contain the message-bus branch of the v ptree. By default, pub and pull endpoints are set to non-meaningful values
        try {
            auto msgbus = v.second.get_child("message-bus");
            pubEndpoint = msgbus.get<std::string>("pub-endpoint", "tcp://127.0.0.1:5678");
            pullEndpoint = msgbus.get<std::string>("pull-endpoint", "tcp://127.0.0.1:1234");
        } catch (pt::ptree_bad_path&) {}

        //loop through each branch of the s7comm tree
        auto devices = v.second.equal_range("s7comm");
        for (auto iter = devices.first; iter != devices.second; ++iter) {
            auto device = iter->second;

            //required for pub sub
            std::shared_ptr<otsim::msgbus::Pusher> pusher;
            std::shared_ptr<otsim::msgbus::Subscriber> sub;

            //each s7comm device will have a name and mode
            std::string name; //arbitrary device name
            std::string mode; //server or client

            //get name
            try {
                name = device.get<std::string>("<xmlattr>.name");
            } catch (pt::ptree_bad_path&) {
                std::cerr << "ERROR: missing name for S7COMM device" << std::endl;
            }

            //get mode (either client or server)
            try {
                mode = device.get<std::string>("<xmlattr>.mode");
            } catch (pt::ptree_bad_path&) {
                std::cerr << "ERROR: missing mode for S7COMM device" << std::endl;
            }

            //if the device specifies a pub endpoint create a msgbus subscriber with that endpoint, otherwise, use the default
            if (device.get_child_optional("pub-endpoint")) {
                auto endpoint = device.get<std::string>("pub-endpoint");
                sub = otsim::msgbus::Subscriber::Create(endpoint);
            } else {
                sub = otsim::msgbus::Subscriber::Create(pubEndpoint);
            }

            //if the device specifies a pull endpoint create a msgbus pusher with that endpoint, otherwise, use the default
            if (device.get_child_optional("pull-endpoint")) {
                auto endpoint = device.get<std::string>("pull-endpoint");
                pusher = otsim::msgbus::Pusher::Create(endpoint);
            } else {
                pusher = otsim::msgbus::Pusher::Create(pullEndpoint);
            }

            //if the s7comm device is a server
            if (mode.compare("server") == 0){
                std::cout << fmt::format("configuring S7COMM server {}", name) << std::endl;

                //create the server object
                otsim::snap7::S7Object Server;
                Server = otsim::snap7::Srv_Create();

                //get the endpoint and set it
                if (device.get_child_optional("endpoint")) {
                    auto endpoint = device.get<std::string>("endpoint");

                    std::string ip = endpoint.substr(0, endpoint.find(":"));
                    //std::uint16_t port = static_cast<std::uint16_t>(stoi(endpoint.substr(endpoint.find(":") + 1)));

                    //set the ip address that the server will start to when started (doesn't start until the end of this loop)
                    otsim::snap7::Srv_StartTo(Server, ip);
                }

                /*
                We need to create a handler here for the server for subscribing purposes.
                A handler can either be a status handler or an update handler, both are types of envelopes.
                Envelopes store a version (string), kind (string), metadata, and contents (typenamed, either
                contains the a vector of the struct status or update).
                */
                otsim::msgbus::StatusHandler statusHandler; //this status handler will have measurements (vectors of points) pushed to it during the XML scan

                //Add the envelope's (statusHandler's) version and kind
                auto version = device.get<std::string>("version", "v1");
                statusHandler.version=version;

                std::string kind = "Status";
                statusHandler.kind=kind;

                sub->AddHandler(statusHandler); //pair the handler with the subscriber

                //loop through the inputs, getting the tag for each
                auto inputs = device.equal_range("input");
                for(auto iter=inputs.first; iter !=inputs.second; iter++){
                    auto point = iter->second;
                    
                    otsim::msgbus::Point p;
                    
                    //get the current tag
                    p.tag = point.get<std::string>("tag");
                    p.value = point.get<double>("value");
                    p.ts = point.get<std::uint64_t>("ts");

                    //create a status object, push the tag to it, push that status object to the statusHandler's contents vector
                    otsim::msgbus::Status status;
                    status.measurements.push_back(p);
                    statusHandler.contents.push_back(status);
                }

                //loop through the outputs, getting the tag for each
                auto outputs = device.equal_range("output");
                for(auto iter=outputs.first; iter !=outputs.second; iter++){
                    auto point = iter->second;
                    
                    otsim::msgbus::Point p;
                    
                    //get the current tag
                    p.tag = point.get<std::string>("tag");
                    p.value = point.get<double>("value");
                    p.ts = point.get<std::uint64_t>("ts");

                    //create a status object, push the tag to it, push that status object to the statusHandler's contents vector
                    otsim::msgbus::Status status;
                    status.measurements.push_back(p);
                    statusHandler.contents.push_back(status);
                }

                //start the server and add it to the vector of servers
                otsim::snap7::Srv_Start(Server);
                servers.push_back(Server);
            } else if (mode.compare("client") == 0){ //if the s7comm device is a client

                //create the client object
                otsim::snap7::S7Object client;
                client = otsim::snap7::Cli_Create();

                //create a channel listener object
                auto listener = Listener::Create(name, pusher);

                /*
                set the connection type for the device based on what is provided in the xml.
                if nothing is provided in the xml, set the connection type of the client to be
                equal to 3. values 3-10 represent s7 basic. 2 represents OP. 1 represenets PG.
                s7 basic is likely the main/only connection type for more clients.
                */
                try {
                    auto connectionType = device.get<std::uint16_t>("connection-type", 3);
                    otsim::snap7::Cli_SetConnectionType(client, connectionType);
                } catch (pt::ptree_bad_path&) {
                    std::cerr << "ERROR: missing mode for CONNECTION TYPE for client s7comm device" << std::endl;
                }

                /*
                set the connection parameters. this includes the address, local TSAP and remote TSAP.
                local TSAP and remote TSAP are stored as 16 bit unsigned integers. address is stored as a pointer
                to an ANSI string; "192.168.1.12" for example. 
                */
                auto ip_address = device.get<std::string>("ip-address", "192.168.0.0"); //get IP, defaults to 192.168.0.0 arbitrarily

                auto local_tsap = device.get<std::uint16_t>("local-tsap", 10.00); //get local TSAP, defaults to 10.00

                auto remote_tsap = device.get<std::uint16_t>("remote-tsap", 13.00); //get remote TSAP, defaults to 13.00 

                otsim::snap7::Cli_SetConnectionParams(client, ip_address, local_tsap, remote_tsap); 

                /*
                declare where the client will connect. for S7 CPUs the default should always be rack 0 slot 2
                */
                auto rack = device.get<std::uint16_t>("rack", 0);
                auto slot = device.get<std::uint16_t>("slot", 2);
                otsim::snap7::Cli_ConnectTo(client, ip_address, rack, slot);

                /*
                We need to create a handler here for the server for subscribing purposes.
                A handler can either be a status handler or an update handler, both are types of envelopes.
                Envelopes store a version (string), kind (string), metadata, and contents (typenamed, either
                contains the a vector of the struct status or update).
                */
                otsim::msgbus::UpdateHandler updateHandler; //this update handler will have updates (vectors of points) pushed to it during the XML scan

                //Add the envelope's (updateHandler's) version and kind
                auto version = device.get<std::string>("version", "v1");
                updateHandler.version=version;

                std::string kind = "Update";
                updateHandler.kind=kind;

                sub->AddHandler(updateHandler); //pair the handler with the subscriber

                //loop through the inputs, getting the tag for each
                auto inputs = device.equal_range("input");
                for(auto iter=inputs.first; iter !=inputs.second; iter++){
                    auto point = iter->second;
                    
                    otsim::msgbus::Point p;
                    
                    //get the current tag, value, and ts
                    p.tag = point.get<std::string>("tag");
                    p.value = point.get<double>("value");
                    p.ts = point.get<std::uint64_t>("ts");

                    //create a status object, push the tag to it, push that status object to the statusHandler's contents vector
                    otsim::msgbus::Update update;

                    //set the update's recipient and confirm fields
                    update.recipient = point.get<std::string>("recipient", "");
                    update.confirm = point.get<std::string>("confirm", "");

                    update.updates.push_back(p);
                    updateHandler.contents.push_back(update);
                }

                //loop through the outputs, getting the tag for each
                auto outputs = device.equal_range("output");
                for(auto iter=outputs.first; iter !=outputs.second; iter++){
                    auto point = iter->second;
                    
                    otsim::msgbus::Point p;
                    
                    //get the current tag
                    p.tag = point.get<std::string>("tag");
                    p.value = point.get<double>("value");
                    p.ts = point.get<std::uint64_t>("ts");

                    //create a status object, push the tag to it, push that status object to the statusHandler's contents vector
                    otsim::msgbus::Update update;

                    //set the update's recipient and confirm fields
                    update.recipient = point.get<std::string>("recipient", "");
                    update.confirm = point.get<std::string>("confirm", "");

                    update.updates.push_back(p);
                    updateHandler.contents.push_back(update);
                }

                //connect
                otsim::snap7::Cli_Connect(client);

                clients.push_back(client);

                listeners.push_back(listener);
            } else {
                std::cerr << "ERROR: invalid mode provided for S7COMM config" << std::endl;
                return 1;
            }

            //push back the subscriber created for this s7comm device
            sub->Start("RUNTIME");
            subscribers.push_back(sub);

        } //end of S7COMM loop
    } //end of BOOST_FOREACH loop

    //signal handling/threading
    std::signal(SIGINT, signalHandler);

    std::unique_lock lk(m);
    cv.wait(lk);

    //this *should* cause any blocking subscribers to immediately return so
    //threads can exit.
    for (auto &sub : subscribers) {
        sub->Stop();
    }

    for (auto &client : clients) {
        otsim::snap7::Cli_Destroy(client);
    }

    for (auto &server : servers) {
        otsim::snap7::Srv_Stop(server);
        otsim::snap7::Srv_Destroy(server);
    }

    return 0;
}