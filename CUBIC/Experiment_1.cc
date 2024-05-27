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

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Experiment_1");

void CwndChange(Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd){
    *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << newCwnd << std::endl;
}

void saveThroughput(Ptr<FlowMonitor> monitor, Ptr<Ipv4FlowClassifier> classifier, Ptr<OutputStreamWrapper> stream1, Ptr<OutputStreamWrapper> stream2, Ipv4Address sender1_addr, Ipv4Address sender2_addr){
    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for(std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); i++){
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        if(t.sourceAddress == sender1_addr){
            double throughput1 = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024;
            *stream1->GetStream() << Simulator::Now().GetSeconds() << "\t" << throughput1 << std::endl;
        }else if(t.sourceAddress == sender2_addr){
            double throughput2 = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024;
            *stream2->GetStream() << Simulator::Now().GetSeconds() << "\t" << throughput2 << std::endl;
        }
    }
    Simulator::Schedule(Seconds(1.0), &saveThroughput, monitor, classifier, stream1, stream2, sender1_addr, sender2_addr);
}

int main(){
    // Routers 2
    NodeContainer routers, sender, receiver, background_traffic1, background_traffic2;;
    routers.Create(2);
    sender.Create(2);
    receiver.Create(2);
    background_traffic1.Create(2);
    background_traffic2.Create(2);

    InternetStackHelper internet;
    internet.Install(routers);
    internet.Install(sender);
    internet.Install(receiver);
    internet.Install(background_traffic1);
    internet.Install(background_traffic2);

    // Point-to-Point
    PointToPointHelper p2p, bottleneck;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    p2p.SetChannelAttribute("Delay", StringValue("35ms"));

    bottleneck.SetDeviceAttribute("DataRate", StringValue("400Mbps"));
    bottleneck.SetChannelAttribute("Delay", StringValue("50ms"));
    
    // Network Device Containers
    NetDeviceContainer senderDevices;
    senderDevices = p2p.Install(routers.Get(0), sender.Get(0));
    senderDevices.Add(p2p.Install(routers.Get(0), sender.Get(1)));

    NetDeviceContainer receiverDevices;
    receiverDevices = p2p.Install(routers.Get(1), receiver.Get(0));
    receiverDevices.Add(p2p.Install(routers.Get(1), receiver.Get(1)));

    NetDeviceContainer background_trafficDevices;
    background_trafficDevices = p2p.Install(routers.Get(0), background_traffic1.Get(0));
    background_trafficDevices.Add(p2p.Install(routers.Get(0), background_traffic1.Get(1)));
    background_trafficDevices.Add(p2p.Install(routers.Get(1), background_traffic2.Get(0)));
    background_trafficDevices.Add(p2p.Install(routers.Get(1), background_traffic2.Get(1)));

    NetDeviceContainer routerDevices;
    routerDevices = bottleneck.Install(routers.Get(0), routers.Get(1));

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer senderInterfaces = address.Assign(senderDevices);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer receiverInterfaces = address.Assign(receiverDevices);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer background_trafficInterfaces = address.Assign(background_trafficDevices);

    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer routerInterfaces = address.Assign(routerDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint64_t bdp = 400/8 * 1024 * 1024 /1000 * 50;
    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::TailDropQueueDisc", "MaxSize", QueueSizeValue(QueueSize(QueueSizeUnit::BYTES, bdp)));
    tch.install(routerDevices);

    uint port = 9;
    // Backgrounf Traffic from background-traffic1 to background-traffic3
    Address sinkAddress1(InetSocketAddress(background_trafficDevices.Get(5), port));
    PacketSinkHelper sinkHelper1("ns3::TcpSocketFactory", sinkAddress1);
    ApplicationContainer sinkApp1 = sinkHelper1.Install(background_traffic2.Get(0));
    sinkApp1.Start(Seconds(0.0));
    sinkApp1.Stop(Seconds(600.0));

    OnOffHelper onOffHelper1("ns3::TcpSocketFactory", sinkAddress1);
    onOffHelper1.SetAttribute("DataRate", DataRateValue(DataRate("100Kbps")));
    onOffHelpe1r.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer onOffApp1 = onOffHelper1.Install(background_traffic1.Get(0));
    onOffAppq.Start(Seconds(1.0));
    onOffAppq.Stop(Seconds(600.0));

    // Background Traffic from background-traffic4 to background-traffic2
    Address sinkAddress2(InetSocketAddress(background_trafficDevices.Get(3), port));
    PacketSinkHelper sinkHelper2("ns3::TcpSocketFactory", sinkAddress2);
    ApplicationContainer sinkApp2 = sinkHelper2.Install(background_traffic2.Get(1));
    sinkApp2.Start(Seconds(0.0));
    sinkApp2.Stop(Seconds(600.0));

    OnOffHelper onOffHelper2("ns3::TcpSocketFactory", sinkAddress2);
    onOffHelper2.SetAttribute("DataRate", DataRateValue(DataRate("100Kbps")));
    onOffHelper2.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer onOffApp2 = onOffHelper2.Install(background_traffic2.Get(1));
    onOffApp2.Start(Seconds(1.0));
    onOffApp2.Stop(Seconds(600.0));

    // Two CUBIC flows from sender to receiver one starting little later
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpCubic"));
    Address sinkAddress3(InetSocketAddress(receiverInterfaces.GetAddress(1), port));
    PacketSinkHelper sinkHelper3("ns3::TcpSocketFactory", sinkAddress3);
    ApplicationContainer sinkApp3 = sinkHelper3.Install(receiver.Get(0));
    sinkApp3.Start(Seconds(0.0));
    sinkApp3.Stop(Seconds(600.0));

    OnOffHelper onOffHelper3("ns3::TcpSocketFactory", sinkAddress3);
    onOffHelper3.SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    onOffHelper3.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer onOffApp3 = onOffHelper3.Install(sender.Get(0));
    onOffApp3.Start(Seconds(0.0));
    onOffApp3.Stop(Seconds(600.0));

    Address sinkAddress4(InetSocketAddress(receiverInterfaces.GetAddress(3), port));
    PacketSinkHelper sinkHelper4("ns3::TcpSocketFactory", sinkAddress4);
    ApplicationContainer sinkApp4 = sinkHelper4.Install(receiver.Get(1));
    sinkApp4.Start(Seconds(100.0));
    sinkApp4.Stop(Seconds(600.0));

    OnOffHelper onOffHelper4("ns3::TcpSocketFactory", sinkAddress4);
    onOffHelper4.SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    onOffHelper4.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer onOffApp4 = onOffHelper4.Install(sender.Get(1));
    onOffApp4.Start(Seconds(100.0));
    onOffApp4.Stop(Seconds(600.0));

    // Monitor the congestion window and throughput of both the flows continuously for 600 seconds using ascii trace files
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream1 = ascii.CreateFileStream("cwnd1.tr");
    Ptr<OutputStreamWrapper> stream2 = ascii.CreateFileStream("cwnd2.tr");
    sender.Get(0)->GetObject<TcpSocketBase>()->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback(&CwndChange, stream1));
    
    sender.Get(1)->GetObject<TcpSocketBase>()->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback(&CwndChange, stream2));

    Ptr<OutputStreamWrapper> stream3 = ascii.CreateFileStream("throughput1.tr");
    Ptr<OutputStreamWrapper> stream4 = ascii.CreateFileStream("throughput2.tr");

    Ptr<Node> node1 = sender.Get(0);
    Ptr<Ipv4> ipv41 = node1->GetObject<Ipv4>();
    Ipv4InterfaceAddress iaddr1 = ipv41->GetAddress(1, 0);
    Ipv4Address sender1_addr = iaddr1.GetLocal();

    Ptr<Node> node2 = sender.Get(1);
    Ptr<Ipv4> ipv42 = node2->GetObject<Ipv4>();
    Ipv4InterfaceAddress iaddr2 = ipv42->GetAddress(1, 0);
    Ipv4Address sender2_addr = iaddr2.GetLocal();


    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    Simulator::Schedule(Seconds(1.0), &saveThroughput, monitor, classifier, stream3, stream4, sender1_addr, sender2_addr);
    
    Simulator::Stop(Seconds(600.0));
    Simulator::Run();
    monitor->CheckForLostPackets();
    Simulator::Destroy();

    return 0;
}