/*
This is the third experiment of the DCTCP lab. In this experiment multiple device connected via a switch with static buffer.
One device acts as client and other n devices as server. The client requests data from the servers and the servers respond with
data with packet of size 1/n MB. This process is done parallely for all the servers. The client sends a request to all the servers.
Once the client receives the data from all the servers, it sends another request to all the servers. This process is repeated for 
1000 times and query response time is calculated. 

Client and Server Applications needs to be implemented. 
*/

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

uint32_t n_servers = 3;
uint32_t reps = 10;
std::ofstream queryTime;

class ServerApp : public Application{
    public:
        ServerApp();
        virtual ~ServerApp();
        void Setup(Address address, uint32_t packetSize);
    private:
        virtual void StartApplication(void);
        virtual void StopApplication(void);
        void HandleRead(Ptr<Socket> socket);
        void HandleAccept(Ptr<Socket> s, const Address& from);
        Address m_address;
        Ptr<Socket> m_socket;
        uint32_t m_packetSize;
};

ServerApp::ServerApp() : m_socket(0), m_packetSize(0){
}

ServerApp::~ServerApp(){
    m_socket = 0;
}

void ServerApp::Setup(Address address, uint32_t packetSize){
    m_address = address;
    m_packetSize = packetSize;
}

void ServerApp::StartApplication(void){
    m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
    m_socket->Bind(m_address);
    m_socket->Listen();
    m_socket->SetAcceptCallback(MakeNullCallback<bool, Ptr<Socket>, const Address&>(), MakeCallback(&ServerApp::HandleAccept, this));
}

void ServerApp::StopApplication(void){
    if(m_socket){
        m_socket->Close();
    }
}

void ServerApp::HandleAccept(Ptr<Socket> s, const Address& from){
    s->SetRecvCallback(MakeCallback(&ServerApp::HandleRead, this));
}

void ServerApp::HandleRead(Ptr<Socket> socket){
    // Accept the packet, if packet size > 0, send new packet back to client with m_packetSize
    Ptr<Packet> packet = socket->Recv();
    
    if(packet->GetSize() > 0){
        NS_LOG_UNCOND("Server returning Packet");
        Ptr<Packet> p = Create<Packet>(m_packetSize);
        socket->Send(p);
    }
}

class ClientApp : public Application{
    public:
        ClientApp();
        virtual ~ClientApp();
        // Setup function takes the vector of server addresses, and n_servers
        void Setup(std::vector<Address> addresses, uint32_t n_servers, uint32_t reps);
        void StartQueries();
    private:
        virtual void StartApplication(void);
        virtual void StopApplication(void);
        void HandleRead(Ptr<Socket> socket);
        void SendQuery();
        std::vector<Address> m_addresses;
        std::vector<Ptr<Socket>> m_sockets;
        uint32_t m_n_servers;
        uint32_t m_reps;
        uint32_t m_received;
        Time m_queryStart;
        // Need a lock to protect m_received
        std::mutex m_mutex;
};

ClientApp::ClientApp() : m_n_servers(0), m_reps(0), m_received(0){
}

ClientApp::~ClientApp(){
}

void ClientApp::Setup(std::vector<Address> addresses, uint32_t n_servers, uint32_t reps){
    m_addresses = addresses;
    m_n_servers = n_servers;
    m_reps = reps;
}

void ClientApp::StartQueries(){
    SendQuery();
}

void ClientApp::StartApplication(void){
    for(uint32_t i=0;i<m_n_servers;i++){
        Ptr<Socket> socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
        socket->SetRecvCallback(MakeCallback(&ClientApp::HandleRead, this));
        socket->Connect(m_addresses[i]);
        m_sockets.push_back(socket);
    }
}

void ClientApp::StopApplication(void){
    for(uint32_t i=0;i<m_n_servers;i++){
        if(m_sockets[i]){
            m_sockets[i]->Close();
        }
    }
}

void ClientApp::SendQuery(){
    if(m_reps-- <= 0){
        return;
    }
    m_queryStart = Simulator::Now();
    NS_LOG_UNCOND("Sending Query at time: "<<m_queryStart.GetSeconds());
    m_received = 0;
    for(uint32_t i=0;i<m_n_servers;i++){
        Ptr<Packet> p = Create<Packet>(10);
        NS_LOG_UNCOND("Sending Query to server of size: "<<p->GetSize());
        m_sockets[i]->Send(p);
    }
}

void ClientApp::HandleRead(Ptr<Socket> socket){
    NS_LOG_UNCOND("Client recieved response from server");
    Ptr<Packet> packet = socket->Recv();
    if(packet->GetSize() > 0){
        m_mutex.lock();
        m_received++;
        if(m_received == m_n_servers){
            Time queryEnd = Simulator::Now();
            // Query time in ms
            double queryTime_ = (queryEnd - m_queryStart).GetMilliSeconds();
            std::cout<<"Query Time: "<<queryTime_<<"\n";
            queryTime<<queryTime_<<"\n";
            // SendQuery();
            Simulator::Schedule(Seconds(10), &ClientApp::SendQuery, this);
        }
        m_mutex.unlock();
    }
}

int main(){
    uint32_t packetSize = 1024*1024/n_servers;
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpDctcp"));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(1024*1024));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(1024*1024));

    NodeContainer nodes;
    nodes.Create((int)n_servers+1);
    Ptr<Node> T = CreateObject<Node>();

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    p2p.SetChannelAttribute("Delay", StringValue("5ms"));
    // Static buffer size of 100 packets
    p2p.SetQueue("ns3::DropTailQueue", "MaxSize", QueueSizeValue(QueueSize("100p")));

    std::vector<NetDeviceContainer> server;
    for(int i=0;i<(int)n_servers;i++){
        NetDeviceContainer dev = p2p.Install(nodes.Get(i), T);
        server.push_back(dev);
    }
    NetDeviceContainer client = p2p.Install(nodes.Get((int)n_servers), T);

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> serverInterface;
    std::vector<Address> serverAddress;
    address.SetBase("10.1.1.0","255.255.255.0");
    for(int i=0;i<(int)n_servers;i++){
        Ipv4InterfaceContainer interface = address.Assign(server[i]);
        serverInterface.push_back(interface);
        serverAddress.push_back(InetSocketAddress(interface.GetAddress(0), 9));
        address.NewNetwork();
    }
    Ipv4InterfaceContainer clientInterface = address.Assign(client);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Create server applications
    NS_LOG_UNCOND("Creating Server Applications");
    std::vector<Ptr<ServerApp>> serverApps;
    for(int i=0;i<(int)n_servers;i++){
        Ptr<ServerApp> app = CreateObject<ServerApp>();
        app->Setup(serverAddress[i], packetSize);
        nodes.Get(i)->AddApplication(app);
        app->SetStartTime(Seconds(1.0));
        app->SetStopTime(Seconds(1000.0));
        serverApps.push_back(app);
    }

    NS_LOG_UNCOND("Creating Client Application");
    // Create client application
    Ptr<ClientApp> clientApp = CreateObject<ClientApp>();
    clientApp->Setup(serverAddress, n_servers, reps);
    nodes.Get(n_servers)->AddApplication(clientApp);
    clientApp->SetStartTime(Seconds(1.0));
    clientApp->SetStopTime(Seconds(1000.0));

    queryTime.open("queryTime.txt");

    Simulator::Schedule(Seconds(1.1), &ClientApp::StartQueries, clientApp);

    Simulator::Stop(Seconds(1000.0));
    Simulator::Run();
    Simulator::Destroy();

    queryTime.close();
    return 0;
}