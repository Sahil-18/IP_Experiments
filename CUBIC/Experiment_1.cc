#include <fstream>
#include <string>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ipv4.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/traffic-control-module.h"
#include <iostream>

#include "Exp_app.h"

using namespace ns3;

# define START_TIME 0.0
# define STOP_TIME 100.0

# define START_TIME1 10.0

NS_LOG_COMPONENT_DEFINE("Experiment_1");

ExperimentApp::ExperimentApp()
    : m_socket(nullptr),
      m_peer(),
      m_packetSize(0),
      m_dataRate(0),
      m_sendEvent(),
      m_running(false)
{
}

ExperimentApp::~ExperimentApp()
{
    m_socket = nullptr;
}

TypeId ExperimentApp::GetTypeId(void)
{
    static TypeId tid = TypeId("ExperimentApp")
        .SetParent<Application>()
        .AddConstructor<ExperimentApp>();
    return tid;
}

void ExperimentApp::Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, DataRate dataRate)
{
    m_socket = socket;
    m_peer = address;
    m_packetSize = packetSize;
    m_dataRate = dataRate;
}

void ExperimentApp::StartApplication()
{
    m_running = true;
    if(m_socket->Bind() == -1){
        NS_LOG_UNCOND("Bind failed");
    }
    if(m_socket->Connect(m_peer) == -1){
        NS_LOG_UNCOND("Connect failed");
    }else{
        NS_LOG_UNCOND("Connected");
    }
    SendPacket();
}

void ExperimentApp::StopApplication()
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

void ExperimentApp::SendPacket()
{
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    m_socket->Send(packet);
    NS_LOG_UNCOND("Sending packet at " << Simulator::Now().GetSeconds());
    ScheduleTx();
}

void ExperimentApp::ScheduleTx()
{
    if (m_running)
    {
        Time tNext(Seconds(m_packetSize * 8 / static_cast<double>(m_dataRate.GetBitRate())));
        m_sendEvent = Simulator::Schedule(tNext, &ExperimentApp::SendPacket, this);
    }
}


static void
CwndChange(Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "\t" << newCwnd);
    *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << oldCwnd << "\t" << newCwnd
                         << std::endl;
}

static void 
PrintFlowStats(Ptr<FlowMonitor> monitor, Ptr<Ipv4FlowClassifier> classifier)
{
    // Print the flow monitor statistics for current second and schedule for next second
    monitor->CheckForLostPackets();

    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress
                  << " Src Port " << t.sourcePort << " Dst Port " << t.destinationPort << std::endl;
        std::cout << "Tx Packets = " << i->second.txPackets << std::endl;
        std::cout << "Rx Packets = " << i->second.rxPackets << std::endl;
        std::cout << "Lost Packets = " << i->second.lostPackets << std::endl;
        std::cout << "Throughput: " << i->second.rxBytes * 8.0 / 1e6 / 20 << " Mbps" << std::endl;
    }

    Simulator::Schedule(Seconds(1), &PrintFlowStats, monitor, classifier);
}

// int
// main(int argc, char* argv[])
// {
//     CommandLine cmd(__FILE__);
//     cmd.Parse(argc, argv);

//     NodeContainer nodes;
//     nodes.Create(2);

//     PointToPointHelper pointToPoint;
//     pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
//     pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

//     NetDeviceContainer devices;
//     devices = pointToPoint.Install(nodes);

//     Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
//     em->SetAttribute("ErrorRate", DoubleValue(0.00001));
//     devices.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));

//     InternetStackHelper stack;
//     stack.Install(nodes);

//     Ipv4AddressHelper address;
//     address.SetBase("10.1.1.0", "255.255.255.252");
//     Ipv4InterfaceContainer interfaces = address.Assign(devices);

//     uint16_t sinkPort = 8080;
//     Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), sinkPort));
//     PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory",
//                                       InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
//     ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1));
//     sinkApps.Start(Seconds(0.));
//     sinkApps.Stop(Seconds(20.));

//     Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());

//     Ptr<ExperimentApp> app = CreateObject<ExperimentApp>();
//     app->Setup(ns3TcpSocket, sinkAddress, 1040, DataRate("1Mbps"));
//     nodes.Get(0)->AddApplication(app);
//     app->SetStartTime(Seconds(1.));
//     app->SetStopTime(Seconds(20.));

//     AsciiTraceHelper asciiTraceHelper;
//     Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("sixth.cwnd");
//     ns3TcpSocket->TraceConnectWithoutContext("CongestionWindow",
//                                              MakeBoundCallback(&CwndChange, stream));

//     FlowMonitorHelper flowmon;
//     Ptr<FlowMonitor> monitor = flowmon.InstallAll();
//     Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
//     Simulator::Stop(Seconds(20));
//     Simulator::Schedule(Seconds(0.1), &PrintFlowStats, monitor, classifier);
//     Simulator::Run();
//     Simulator::Destroy();

//     return 0;
// }

int
main(int argc, char* argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    NodeContainer sender, receiver, router;
    sender.Create(2);
    receiver.Create(2);
    router.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer senderDevices, receiverDevices, routerDevices;
    senderDevices = pointToPoint.Install(sender.Get(0), router.Get(0));
    senderDevices.Add(pointToPoint.Install(sender.Get(1), router.Get(0)));

    receiverDevices = pointToPoint.Install(receiver.Get(0), router.Get(1));
    receiverDevices.Add(pointToPoint.Install(receiver.Get(1), router.Get(1)));

    pointToPoint.SetDeviceAttribute("DataRate", StringValue("400Kbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

    routerDevices = pointToPoint.Install(router.Get(0), router.Get(1));

    Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
    em->SetAttribute("ErrorRate", DoubleValue(0.00001));
    receiverDevices.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em));

    InternetStackHelper stack;
    stack.Install(sender);
    stack.Install(receiver);
    stack.Install(router);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer senderInterfaces = address.Assign(senderDevices);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer receiverInterfaces = address.Assign(receiverDevices);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer routerInterfaces = address.Assign(routerDevices);

    // Enable dynamic global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(receiverInterfaces.GetAddress(0), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory",
                                      InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(receiver.Get(0));
    sinkApps.Start(Seconds(0.));
    sinkApps.Stop(Seconds(10.));

    Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(sender.Get(0), TcpSocketFactory::GetTypeId());

    Ptr<ExperimentApp> app = CreateObject<ExperimentApp>();
    app->Setup(ns3TcpSocket, sinkAddress, 1040, DataRate("1Mbps"));
    sender.Get(0)->AddApplication(app);
    app->SetStartTime(Seconds(1.));
    app->SetStopTime(Seconds(10.));

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("sixth.cwnd");
    ns3TcpSocket->TraceConnectWithoutContext("CongestionWindow",
                                             MakeBoundCallback(&CwndChange, stream));

    Simulator::Stop(Seconds(10));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

// int main(){
//     // Routers 2
//     NodeContainer routers, sender, receiver, background_traffic1, background_traffic2;;
//     sender.Create(2);
//     routers.Create(2);
//     receiver.Create(2);
//     background_traffic1.Create(2);
//     background_traffic2.Create(2);

//     InternetStackHelper internet;
//     internet.Install(routers);
//     internet.Install(sender);
//     internet.Install(receiver);
//     internet.Install(background_traffic1);
//     internet.Install(background_traffic2);

//     // Point-to-Point
//     PointToPointHelper p2p, bottleneck;
//     p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
//     p2p.SetChannelAttribute("Delay", StringValue("35ms"));

//     uint64_t bdp = 400/8 * 1024 * 1024 /1000 * 50;
//     bottleneck.SetDeviceAttribute("DataRate", StringValue("400Mbps"));
//     bottleneck.SetChannelAttribute("Delay", StringValue("50ms"));
//     bottleneck.SetQueue("ns3::DropTailQueue", "MaxSize", QueueSizeValue(QueueSize(QueueSizeUnit::BYTES, bdp)));
    
//     // Network Device Containers
//     NetDeviceContainer senderDevices;
//     senderDevices = p2p.Install(routers.Get(0), sender.Get(0));
//     senderDevices.Add(p2p.Install(routers.Get(0), sender.Get(1)));

//     NetDeviceContainer receiverDevices;
//     receiverDevices = p2p.Install(routers.Get(1), receiver.Get(0));
//     receiverDevices.Add(p2p.Install(routers.Get(1), receiver.Get(1)));

//     NetDeviceContainer background_trafficDevices1, background_trafficDevices2;
//     background_trafficDevices1 = p2p.Install(routers.Get(0), background_traffic1.Get(0));
//     background_trafficDevices1.Add(p2p.Install(routers.Get(0), background_traffic1.Get(1)));
//     background_trafficDevices2 = p2p.Install(routers.Get(1), background_traffic2.Get(0));
//     background_trafficDevices2.Add(p2p.Install(routers.Get(1), background_traffic2.Get(1)));

//     NetDeviceContainer routerDevices;
//     routerDevices = bottleneck.Install(routers.Get(0), routers.Get(1));

//     Ipv4AddressHelper address;
//     address.SetBase("10.1.1.0", "255.255.255.0");
//     Ipv4InterfaceContainer senderInterfaces = address.Assign(senderDevices);

//     address.SetBase("10.1.2.0", "255.255.255.0");
//     Ipv4InterfaceContainer receiverInterfaces = address.Assign(receiverDevices);

//     address.SetBase("10.1.3.0", "255.255.255.0");
//     Ipv4InterfaceContainer background_trafficInterfaces1 = address.Assign(background_trafficDevices1);

//     address.SetBase("10.1.4.0", "255.255.255.0");
//     Ipv4InterfaceContainer background_trafficInterfaces2 = address.Assign(background_trafficDevices2);

//     address.SetBase("10.1.5.0", "255.255.255.0");
//     Ipv4InterfaceContainer routerInterfaces = address.Assign(routerDevices);

//     Ipv4GlobalRoutingHelper::PopulateRoutingTables();

//     uint port = 9;
//     // Backgrounf Traffic from background-traffic1 to background-traffic3
//     // Address srcAddress1(InetSocketAddress(background_trafficInterfaces1.GetAddress(1), port));
//     // OnOffHelper onOffHelper1("ns3::TcpSocketFactory", srcAddress1);
//     // onOffHelper1.SetAttribute("DataRate", DataRateValue(DataRate("100Kbps")));
//     // onOffHelper1.SetAttribute("PacketSize", UintegerValue(1024));
//     // AddressValue remoteAddress1(InetSocketAddress(background_trafficInterfaces2.GetAddress(1), port));
//     // onOffHelper1.SetAttribute("Remote", remoteAddress1);

//     // ApplicationContainer onOffApp1 = onOffHelper1.Install(background_traffic2.Get(1));
//     // onOffApp1.Start(Seconds(START_TIME));
//     // onOffApp1.Stop(Seconds(STOP_TIME));

//     // // Create a packet sink to receive these packets
//     // Address sinkAddress1(InetSocketAddress(background_trafficInterfaces2.GetAddress(1), port));
//     // PacketSinkHelper sinkHelper1("ns3::TcpSocketFactory", sinkAddress1);
//     // ApplicationContainer sinkApp1 = sinkHelper1.Install(background_traffic2.Get(0));
//     // sinkApp1.Start(Seconds(START_TIME));
//     // sinkApp1.Stop(Seconds(STOP_TIME));

//     // // Backgrounf Traffic from background-traffic4 to background-traffic2
//     // Address srcAddress2(InetSocketAddress(background_trafficInterfaces2.GetAddress(3), port));
//     // OnOffHelper onOffHelper2("ns3::TcpSocketFactory", srcAddress2);
//     // onOffHelper2.SetAttribute("DataRate", DataRateValue(DataRate("100Kbps")));
//     // onOffHelper2.SetAttribute("PacketSize", UintegerValue(1024));
//     // AddressValue remoteAddress2(InetSocketAddress(background_trafficInterfaces1.GetAddress(3), port));
//     // onOffHelper2.SetAttribute("Remote", remoteAddress2);

//     // ApplicationContainer onOffApp2 = onOffHelper2.Install(background_traffic1.Get(1));
//     // onOffApp2.Start(Seconds(START_TIME));
//     // onOffApp2.Stop(Seconds(STOP_TIME));

//     // // Create a packet sink to receive these packets
//     // Address sinkAddress2(InetSocketAddress(background_trafficInterfaces1.GetAddress(3), port));
//     // PacketSinkHelper sinkHelper2("ns3::TcpSocketFactory", sinkAddress2);
//     // ApplicationContainer sinkApp2 = sinkHelper2.Install(background_traffic1.Get(1));
//     // sinkApp2.Start(Seconds(START_TIME));
//     // sinkApp2.Stop(Seconds(STOP_TIME));

//     // Two CUBIC flows from sender to receiver one starting little later
//     Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpCubic"));
    
//     Address sinkAddress3(InetSocketAddress(receiverInterfaces.GetAddress(0), port));
//     PacketSinkHelper sinkHelper3("ns3::TcpSocketFactory", sinkAddress3);
//     ApplicationContainer sinkApp3 = sinkHelper3.Install(receiver.Get(0));
//     sinkApp3.Start(Seconds(START_TIME));
//     sinkApp3.Stop(Seconds(STOP_TIME));

//     Ptr<Socket> senderSocket1 = Socket::CreateSocket(sender.Get(0), TcpSocketFactory::GetTypeId());

//     Ptr<ExperimentApp> app1 = CreateObject<ExperimentApp>();
//     app1->Setup(senderSocket1, sinkAddress3, 1024, DataRate("1Gbps"));
//     sender.Get(0)->AddApplication(app1);
//     app1->SetStartTime(Seconds(START_TIME1));
//     app1->SetStopTime(Seconds(STOP_TIME));

//     AsciiTraceHelper ascii;
//     Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("Experiment1.txt");
//     NS_LOG_UNCOND("Experiment 1");
//     senderSocket1->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback(&CwndChange, stream));

//     FlowMonitorHelper flowmon;
//     Ptr<FlowMonitor> monitor = flowmon.InstallAll();
//     std::cout<<"flowmon initiated"<<std::endl;

//     Simulator::Stop(Seconds(STOP_TIME));
//     Simulator::Run();
//     Simulator::Destroy();
//     std::cout << "Simulation Finished" << std::endl;
//     monitor->SerializeToXmlFile("ns3simulator.flowmon", true, true);
//     return 0;
// }