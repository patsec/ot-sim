#include <iostream>
#include <mutex>
#include <thread>

#include <boost/foreach.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include "msgbus/pusher.hpp"
#include "msgbus/subscriber.hpp"

#include "snap7/release/Wrappers/c-cpp/snap7.h"

//for multithreading, mutex
std::condition_variable cv;
std::mutex m;

void signalHandler(int) {
    //if threads waiting for *this, notify_one unblocks one of the waiting threads
    cv.notify_one();
}

int main(int argc, char** argv){
    //argv will contain the path to the XML config file, if argc is less than 2, it means no path was included and the module cannot run
    if (argc < 2) {
        std::cerr << "ERROR: missing path to XML config file" << std::endl;
        return 1;
    }

    //keep servers in scope so their threads don't terminate immediately.
    std::vector<std::shared_ptr<otsim::Snap7::S7Object>> servers;         

    //keep clients in scope so their threads don't terminate immediately.
    std::vector<std::shared_ptr<otsim::Snap7::S7Object>> clients;

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
                otsim::Snap7::S7Object Server;
                Server = otsim::Snap7::Srv_Create();

                //get the endpoint and set it
                if (device.get_child_optional("endpoint")) {
                    auto endpoint = device.get<std::string>("endpoint");

                    std::string ip = endpoint.substr(0, endpoint.find(":"));
                    std::uint16_t port = static_cast<std::uint16_t>(stoi(endpoint.substr(endpoint.find(":") + 1)));

                    auto ip_endpoint = opendnp3::IPEndpoint(ip, port);

                    auto mode = device.get<std::string>("endpoint.<xmlattr>.accept-mode", "CloseNew");
                    auto acceptMode = opendnp3::ServerAcceptModeSpec::from_string(mode);

                    //set the ip address that the server will start to when started (doesn't start until the end of this loop)
                    otsim::Snap7::Srv_StartTo(Server, ip_endpoint);
                }


                //loop through the inputs, getting the tag for each
                auto inputs = device.equal_range("input");
                for(auto iter=inputs.first; iter !=inputs.second; iter++){
                    
                }

                //loop through the outputs, getting the tag for each
                auto outputs = device.equal_range("output");
                for(auto iter=outputs.first; iter !=outputs.second; iter++){
                    
                }

                //start the server and add it to the vector of servers
                otsim::Snap7::Srv_Start(Server);
                servers.push_back(Server);
            } else if (mode.compare("client") == 0){ //if the s7comm device is a client

                //create the client object
                otsim::Snap7::S7Object Client;
                Client = otsim::Snap7::Cli_Create();

                /*
                set the connection type for the device based on what is provided in the xml.
                if nothing is provided in the xml, set the connection type of the client to be
                equal to 3. values 3-10 represent s7 basic. 2 represents OP. 1 represenets PG.
                s7 basic is likely the main/only connection type for more clients.
                */
                try {
                    auto connectionType = device.get<uint16_t>("connection-type", 3);
                    otsim::Snap7::Cli_SetConnectionType(Client, connectionType);
                } catch (pt::ptree_bad_path&) {
                    std::cerr << "ERROR: missing mode for CONNECTION TYPE for client s7comm device" << std::endl;
                }

                /*
                set the connection parameters. this includes the address, local TSAP and remote TSAP.
                local TSAP and remote TSAP are stored as 16 bit unsigned integers. address is stored as a pointer
                to an ANSI string; "192.168.1.12" for example. 
                */
                auto ip_address = device.get<uint16_t>("ip-address", "192.168.0.0"); //get IP, defaults to 192.168.0.0 arbitrarily

                auto local_tsap = device.get<uint16_t>("local-tsap", 10.00); //get local TSAP, defaults to 10.00

                auto remote_tsap = device.get<uint16_t>("remote-tsap", 13.00); //get remote TSAP, defaults to 13.00 

                otsim::Snap7::Cli_SetConnectionParams(Client, ip_address, local_tsap, remote_tsap); 

                /*
                declare where the client will connect. for S7 CPUs the default should always be rack 0 slot 2
                */
                auto rack = device.get<uint16_t>("rack", 0);
                auto slot = device.get<uint16_t>("slot", 2);
                otsim::Snap7::Cli_ConnectTo(Client, ip_address, rack, slot);


                //loop through the inputs, getting the tag for each
                auto inputs = device.equal_range("input");
                for(auto iter=inputs.first; iter !=inputs.second; iter++){
                    
                }

                //loop through the outputs, getting the tag for each
                auto outputs = device.equal_range("output");
                for(auto iter=outputs.first; iter !=outputs.second; iter++){
                    
                }

                //connect
                otsim::Snap7::Cli_Connect(Client);

                clients.push_back(Client);
            } else {
                std::cerr << "ERROR: invalid mode provided for S7COMM config" << std::endl;
                return 1;
            }

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
        otsim::Snap7::Cli_Destroy(client);
    }

    for (auto &server : servers) {
        otsim::Snap7::Srv_Stop(server);
        otsim::Snap7::Srv_Destroy(server);
    }

    return 0;
}