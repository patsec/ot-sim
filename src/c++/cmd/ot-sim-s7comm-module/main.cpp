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

#include "s7/server.hpp"
#include "s7/client.hpp"

#include "snap7.h"

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
The main difference between this listener and the dnp3 channel listener is that this one does not deal with threading
because snap7 does not implement channels or channel states, which dnp3 bases its threading on.
There is still a thread that is used to run the channel listener, but it does not have any mutex implementation, so it should just work
normally.
This class should accomplish all pushing and interface through zeromq via the msgbus pusher class
*/
class Listener {
public:
    static std::shared_ptr<Listener> Create(std::string name, Pusher pusher) {
        return std::make_shared<Listener>(name, pusher);
    }

    //declaration sets the incoming name and pusher
    Listener(std::string name, Pusher pusher) : name(name), pusher(pusher) {
        thread = std::thread(std::bind(&Listener::Run, this));
    }

    ~Listener() {};

    void runThread(){
        thread = std::thread(std::bind(&Listener::Run, this));
    }

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

        //create an envelope with all of the data
        auto env = otsim::msgbus::NewEnvelope(name, contents);
        
        //push all data to msgbus
        pusher->Push("RUNTIME", env);
    }

    std::string name;
    Pusher      pusher;
    std::thread thread;
};

int main(int argc, char** argv){
    //argv will contain the path to the XML config file, if argc is less than 2, it means no path was included and the module cannot run
    if (argc < 2) {
        std::cerr << "ERROR: missing path to XML config file" << std::endl;
        return 1;
    }

    //keep servers in scope so their threads don't terminate immediately.
    std::vector<std::shared_ptr<TS7Server>> servers;         

    //keep clients in scope so their threads don't terminate immediately.
    std::vector<std::shared_ptr<TS7Client>> clients;

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
        } catch (pt::ptree_bad_path&) {std::cout<<"\nmessage bus pub/pull endpoint failure\n";}

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

                //auto listener = Listener::Create(name, pusher);

                //create the server object
                auto server = std::make_shared<TS7Server>(); 
                std::string address = "0.0.0.0";
                std::string ip;

                //get the endpoint and set it
                if (device.get_child_optional("endpoint")) {
                    auto endpoint = device.get<std::string>("endpoint");

                    ip = endpoint.substr(0, endpoint.find(":"));
                    address = ip;

                    std::cout<<"S7 server IP: " << ip << std::endl;
                }else{
                    std::cout<<"S7 server IP: 0.0.0.0\n";
                    ip = address;
                }

                otsim::s7::ServerConfig config = {
                    .id = name,
                    .address = device.get<std::uint16_t>("local-address", 1024),
                };

                // TODO: FINISH XML for loop to get DB information for the server
                /*
                auto DBinputs = device.equal_range("DB");

                //loop through each database listed in the XML
                for(auto iter=DBinputs.first; iter !=DBinputs.second; iter++){
                    auto DBinput = iter->second;
                    std::string dbName;
                    uint16_t dbSize;

                    //get DB name (arbitrary)
                    dbName = DBinput.get<std::string>("name");

                    //get DB size
                    dbSize = DBinput.get<uint16_t>("size");
                }*/

                //create a handler using the server class from the s7 folder, which links snap7 and msgbus
                auto s7server = otsim::s7::Server::Create(config, pusher);
                sub->AddHandler(std::bind(&otsim::s7::Server::HandleMsgBusStatus, s7server, std::placeholders::_1));

                //loop through the inputs, getting the tag for each
                auto inputs = device.equal_range("input");
                for(auto iter=inputs.first; iter !=inputs.second; iter++){
                    auto point = iter->second;

                    std::string typ;

                    //get the type of the input/output (binary or analog)
                    try {
                        typ = point.get<std::string>("<xmlattr>.type");
                    } catch (pt::ptree_bad_path&) {
                        std::cerr << "ERROR: missing type for S7COMM input" << std::endl;
                        continue;
                    }

                    if(typ.compare("binary")== 0){
                        otsim::s7::BinaryInputPoint p;
                    
                        //get input information
                        p.tag = point.get<std::string>("tag");
                        p.address = point.get<std::uint16_t>("address");

                        //set input information
                        s7server->AddBinaryInput(p);
                    } else if(typ.compare("analog")== 0){
                        otsim::s7::AnalogInputPoint p;
                    
                        //get input information
                        p.tag = point.get<std::string>("tag");
                        p.address = point.get<std::uint16_t>("address");

                        //set input information
                        s7server->AddAnalogInput(p);
                    } else {
                        std::cerr << "ERROR: invalid type " << typ << " provided for S7COMM input" << std::endl;
                        continue;
                    }
                }

                //loop through the outputs, getting the tag for each. if it is an output, the sbo value will be set to 'true'
                auto outputs = device.equal_range("output");
                for(auto iter=outputs.first; iter !=outputs.second; iter++){
                    auto point = iter->second;

                    std::string typ;

                    //get the type of the input/output (binary or analog)
                    try {
                        typ = point.get<std::string>("<xmlattr>.type");
                    } catch (pt::ptree_bad_path&) {
                        std::cerr << "ERROR: missing type for S7COMM output" << std::endl;
                        continue;
                    }

                    if(typ.compare("binary")== 0){
                        otsim::s7::BinaryOutputPoint p;
                    
                        //get output information
                        p.tag = point.get<std::string>("tag");
                        p.address = point.get<std::uint16_t>("address");
                        p.sbo = point.get<bool>("sbo", 0) == 1;

                        //set output information
                        s7server->AddBinaryOutput(p);
                    } else if(typ.compare("analog")== 0){
                        otsim::s7::AnalogOutputPoint p;
                    
                        //get output information
                        p.tag = point.get<std::string>("tag");
                        p.address = point.get<std::uint16_t>("address");
                        p.sbo = point.get<bool>("sbo", 0) == 1;

                        //set output information
                        s7server->AddAnalogOutput(p);
                    } else {
                        std::cerr << "ERROR: invalid type " << typ << " provided for S7COMM output" << std::endl;
                        continue;
                    }
                }
                std::cout << fmt::format("starting S7comm server {}", name) << std::endl;
                //start the server and add it to the vector of servers
                server->StartTo(ip.c_str());
                sub->Start("RUNTIME");
                subscribers.push_back(sub);
                s7server->Run(server);
                servers.push_back(server);
                //listeners.push_back(listener);
            } else if (mode.compare("client") == 0){ //if the s7comm device is a client

                //create the client object
                auto client = std::make_shared<TS7Client>();

                //create a listener for publishing purposes
                auto listener = Listener::Create(name, pusher);

                std::cout << fmt::format("configuring S7COMM client {}", name) << std::endl;
                //listener->Run();
                
                /*
                set the connection type for the device based on what is provided in the xml.
                if nothing is provided in the xml, set the connection type of the client to be
                equal to 3. values 3-10 represent s7 basic. 2 represents OP. 1 represenets PG.
                s7 basic is likely the main/only connection type for more clients.
                */
                try {
                    auto connectionType = device.get<std::uint16_t>("connection-type", 3);
                    client->SetConnectionType(connectionType);
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

                client->SetConnectionParams(ip_address.c_str(), local_tsap, remote_tsap);
                
                /*
                declare where the client will connect. for S7 CPUs the default is rack 0 slot 2
                */
                auto rack = device.get<std::uint16_t>("rack", 0);
                auto slot = device.get<std::uint16_t>("slot", 2);

                std::string cliId = device.get<std::string>("<xmlattr>.name", "s7-client");
                std::uint16_t cliAddr = device.get<std::uint16_t>("address", 0);

                auto s7client = otsim::s7::Client::Create(cliId, pusher);

                //create an object of the client class from the s7 folder, which links snap to msgbus
                s7client->BuildConfig(cliId, cliAddr);
                
                //add the s7client to subscriber as a handler. the s7client will call handlemsgbusupdate with one input (an envelope)
                sub->AddHandler(std::bind(&otsim::s7::Client::HandleMsgBusUpdate, s7client, std::placeholders::_1));

                //loop through the inputs, getting the tag for each
                auto inputs = device.equal_range("input");
                for(auto iter=inputs.first; iter !=inputs.second; iter++){
                    auto point = iter->second;

                    std::string typ;

                    //get the type of input (binary or analog)
                    try {
                        typ = point.get<std::string>("<xmlattr>.type");
                    } catch (pt::ptree_bad_path&) {
                        std::cerr << "ERROR: missing type for S7COMM input" << std::endl;
                        continue;
                    }

                    if(typ.compare("binary")== 0){   
                        //based on the xml, add tags that the s7client will communicate to the msgbus
                        std::string tag_read = point.get<std::string>("tag");
                        std::uint16_t address_read = point.get<std::uint16_t>("address");

                        s7client->AddBinaryTag(address_read, tag_read);
                    } else if(typ.compare("analog")== 0){
                        //based on the xml, add tags that the s7client will communicate to the msgbus
                        std::string tag_read = point.get<std::string>("tag");
                        std::uint16_t address_read = point.get<std::uint16_t>("address");

                        s7client->AddAnalogTag(address_read, tag_read);
                    } else{
                        std::cerr << "ERROR: invalid type " << typ << " provided for S7COMM input" << std::endl;
                        continue;
                    }
                }

                //loop through the outputs, getting the tag for each
                auto outputs = device.equal_range("output");
                for(auto iter=outputs.first; iter !=outputs.second; iter++){
                    auto point = iter->second;

                    std::string typ;

                    //get the type of output (binary or analog)
                    try {
                        typ = point.get<std::string>("<xmlattr>.type");
                    } catch (pt::ptree_bad_path&) {
                        std::cerr << "ERROR: missing type for S7COMM output" << std::endl;
                        continue;
                    }

                    if(typ.compare("binary")== 0){
                        //based on the xml, add tags that the s7client will communicate to the msgbus
                        std::string tag_read = point.get<std::string>("tag");
                        std::uint16_t address_read = point.get<std::uint16_t>("address");
                        auto sbo  = point.get<bool>("sbo", 0) == 1;

                        s7client->AddBinaryTag(address_read, tag_read, sbo);
                    } else if(typ.compare("analog")== 0){
                        //based on the xml, add tags that the s7client will communicate to the msgbus
                        std::string tag_read = point.get<std::string>("tag");
                        std::uint16_t address_read = point.get<std::uint16_t>("address");
                        auto sbo  = point.get<bool>("sbo", 0) == 1;

                        s7client->AddAnalogTag(address_read, tag_read, sbo);
                    } else{
                        std::cerr << "ERROR: invalid type " << typ << " provided for S7COMM output" << std::endl;
                        continue;
                    }
                }

                std::cout << fmt::format("starting S7comm client {}", cliId) << std::endl;
                sub->Start("RUNTIME");
                subscribers.push_back(sub);
                //connect
                client->ConnectTo(ip_address.c_str(), rack, slot); 
                clients.push_back(client); //<------ THIS LINE IS NEVER REACHED
                //listener->Run();
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

    return 0;
}