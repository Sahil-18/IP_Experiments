#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/tcp-socket-base.h"
#include <fstream>

using namespace ns3;

uint32_t PacketSize = 1024;

std::ofstream cwnd1;
std::ofstream cwnd2;

NS_LOG_COMPONENT_DEFINE("FifthScriptExample");

class TutorialApp : public Application
{
public:
    TutorialApp();
    virtual ~TutorialApp();
    static TypeId GetTypeId();
    void Setup(Ptr<Socket> socket,
               Address address,
               uint32_t packetSize,
               DataRate dataRate);

private:
    virtual void StartApplication();
    virtual void StopApplication();
    void ScheduleTx();
    void SendPacket();

    Ptr<Socket> m_socket;
    Address m_peer;
    uint32_t m_packetSize;
    DataRate m_dataRate;
    EventId m_sendEvent;
    bool m_running;
    uint32_t m_packetsSent;
};

TutorialApp::TutorialApp()
    : m_socket(nullptr),
      m_peer(),
      m_packetSize(0),
      m_dataRate(0),
      m_sendEvent(),
      m_running(false),
      m_packetsSent(0)
{
}

TutorialApp::~TutorialApp()
{
    m_socket = nullptr;
}

TypeId
TutorialApp::GetTypeId()
{
    static TypeId tid = TypeId("TutorialApp")
                            .SetParent<Application>()
                            .SetGroupName("Tutorial")
                            .AddConstructor<TutorialApp>();
    return tid;
}

void
TutorialApp::Setup(Ptr<Socket> socket,
                   Address address,
                   uint32_t packetSize,
                   DataRate dataRate)
{
    m_socket = socket;
    m_peer = address;
    m_packetSize = packetSize;
    m_dataRate = dataRate;
}

void
TutorialApp::StartApplication()
{
    m_running = true;
    m_packetsSent = 0;
    m_socket->Bind();
    m_socket->Connect(m_peer);
    SendPacket();
}

void
TutorialApp::StopApplication()
{
    m_running = false;

    if (m_sendEvent.IsRunning())
    {
        Simulator::Cancel(m_sendEvent);
    }

    if (m_socket)
    {
        m_socket->Close();
    }
}

void
TutorialApp::SendPacket()
{
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    m_socket->Send(packet);
    ScheduleTx();
}

void
TutorialApp::ScheduleTx()
{
    if (m_running)
    {
        Time tNext(Seconds(m_packetSize * 8 / static_cast<double>(m_dataRate.GetBitRate())));
        std::cout<<"Time: "<<Simulator::Now().GetSeconds()<<"s "<< "Next Packet in: "<<tNext.GetSeconds()<<"s\n";
        m_sendEvent = Simulator::Schedule(tNext, &TutorialApp::SendPacket, this);
    }
}

static void
CwndChange(uint32_t oldCwnd, uint32_t newCwnd)
{
    cwnd1<<Simulator::Now().GetSeconds() << "\t" << newCwnd/PacketSize<<std::endl;
}

static void
CwndChange1(uint32_t oldCwnd, uint32_t newCwnd)
{
    cwnd2<< Simulator::Now().GetSeconds() << "\t" << newCwnd/PacketSize<<std::endl;
}

int
main(int argc, char* argv[])
{
    LogComponentEnable("FifthScriptExample", LOG_LEVEL_INFO);

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpCubic"));
    Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(1));
    Config::SetDefault("ns3::TcpL4Protocol::RecoveryType",
                       TypeIdValue(TypeId::LookupByName("ns3::TcpClassicRecovery")));

    NodeContainer senders, receivers, routers;
    senders.Create(2);
    receivers.Create(2);
    routers.Create(2);

    PointToPointHelper pointToPoint, bottleneck; 
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("40ms"));

    bottleneck.SetDeviceAttribute("DataRate", StringValue("400Mbps"));
    bottleneck.SetChannelAttribute("Delay", StringValue("40ms"));
    bottleneck.SetQueue("ns3::DropTailQueue", "MaxSize", QueueSizeValue (QueueSize ("4000p")));

    NetDeviceContainer sender1 = pointToPoint.Install(senders.Get(0), routers.Get(0));
    NetDeviceContainer sender2 = pointToPoint.Install(senders.Get(1), routers.Get(0));
    NetDeviceContainer receiver1 = pointToPoint.Install(receivers.Get(0), routers.Get(1));
    NetDeviceContainer receiver2 = pointToPoint.Install(receivers.Get(1), routers.Get(1));
    NetDeviceContainer router = bottleneck.Install(routers.Get(0), routers.Get(1));

    Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
    em->SetAttribute("ErrorRate", DoubleValue(0.000001));
    receiver1.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em));
    receiver2.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em));

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.252");
    Ipv4InterfaceContainer sender_interface1 = address.Assign(sender1);
    address.NewNetwork();
    Ipv4InterfaceContainer sender_interfaces2 = address.Assign(sender2);
    address.NewNetwork();
    Ipv4InterfaceContainer receiver_interfaces1 = address.Assign(receiver1);
    address.NewNetwork();
    Ipv4InterfaceContainer receiver_interfaces2 = address.Assign(receiver2);
    address.NewNetwork();
    Ipv4InterfaceContainer router_interfaces = address.Assign(router);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(receiver_interfaces1.GetAddress(0), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory",
                                      InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(receivers.Get(0));
    sinkApps.Start(Seconds(0.));
    sinkApps.Stop(Seconds(20.));

    Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(senders.Get(0), TcpSocketFactory::GetTypeId());
    ns3TcpSocket->TraceConnectWithoutContext("CongestionWindow", MakeCallback(&CwndChange));

    Ptr<TutorialApp> app = CreateObject<TutorialApp>();
    app->Setup(ns3TcpSocket, sinkAddress, PacketSize, DataRate("1Gbps"));
    senders.Get(0)->AddApplication(app);
    app->SetStartTime(Seconds(1.));
    app->SetStopTime(Seconds(20.));

    Address sinkAddress1(InetSocketAddress(receiver_interfaces2.GetAddress(0), sinkPort));
    PacketSinkHelper packetSinkHelper1("ns3::TcpSocketFactory",
                                      InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps1 = packetSinkHelper1.Install(receivers.Get(1));
    sinkApps1.Start(Seconds(0.));
    sinkApps1.Stop(Seconds(20.));

    Ptr<Socket> ns3TcpSocket1 = Socket::CreateSocket(senders.Get(1), TcpSocketFactory::GetTypeId());
    ns3TcpSocket1->TraceConnectWithoutContext("CongestionWindow", MakeCallback(&CwndChange1));

    Ptr<TutorialApp> app1 = CreateObject<TutorialApp>();
    app1->Setup(ns3TcpSocket1, sinkAddress1, PacketSize, DataRate("1Gbps"));
    senders.Get(1)->AddApplication(app1);
    app1->SetStartTime(Seconds(1.));
    app1->SetStopTime(Seconds(20.));

    cwnd1.open("cwnd1.txt");
    cwnd2.open("cwnd2.txt");

    // Flow Monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(20));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (const auto& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        std::cout << "Flow ID: " << flow.first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress
                  << " Src Port " << t.sourcePort << " Dst Port " << t.destinationPort << std::endl;
        std::cout << "Tx Packets = " << flow.second.txPackets << std::endl;
        std::cout << "Rx Packets = " << flow.second.rxPackets << std::endl;
        std::cout << "Lost Packets = " << flow.second.lostPackets << std::endl;
        // Throughput in Gbps
        std::cout << "Throughput: " << flow.second.rxBytes * 8.0 / 20.0 / 1000000000.0 << " Gbps" << std::endl;
    }

    Simulator::Destroy();
    cwnd1.close();
    cwnd2.close();
    return 0;
}
