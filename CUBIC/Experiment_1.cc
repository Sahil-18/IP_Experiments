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
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Experiment_1");

void CwndChange(Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd){
    *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << newCwnd << std::endl;
}

void saveThroughput(Ptr<FlowMonitor> monitor, Ptr<IPv4FlowClassifier> classifier, Ptr<OutputStreamWrapper> stream1, Ptr<OutputStreamWrapper> stream2, Ipv4Address sender1_addr, Ipv4Address sender2_add){
    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for(std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); i++){
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        if(t.sourceAddress == sender1_addr){
            double throughput1 = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024;
            *stream1->GetStream() << Simulator::Now().GetSeconds() << "\t" << throughput1 << std::endl;
        }
        if(t.sourceAddress == sender2_addr){
            double throughput2 = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024;
            *stream2->GetStream() << Simulator::Now().GetSeconds() << "\t" << throughput2 << std::endl;
        }
    }
    Simulator::Schedule(Seconds(1.0), &saveThroughput, monitor, classifier, throughput1, throughput2, sender1_addr, sender2_addr);
}

int main(){
    // Routers 2
    NodeContainer routers, sender, receiver, background-traffic1, background-traffic2;;
    routers.Create(2);
    sender.Create(2);
    receiver.Create(2);
    background-traffic1.Create(2);
    background-traffic2.Create(2);

    InternetStackHelper internet;
    routers.Install(internet);
    sender.Install(internet);
    receiver.Install(internet);
    background-traffic1.Install(internet);
    background-traffic2.Install(internet);

    // Point-to-Point
    PointToPointHelper p2p, bottleneck;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    p2p.SetChannelAttribute("Delay", StringValue("35ms"));

    bottleneck.SetDeviceAttribute("DataRate", StringValue("400Mbps"));
    bottleneck.SetChannelAttribute("Delay", StringValue("50ms"));
    
    // Network Device Containers
    NetDeviceContainer senderDevices;
    senderDevices = p2p.Install(routers.Get(0), sender.Get(0));
    senderDevices.add(p2p.Install(routers.Get(0), sender.Get(1)));

    NetDeviceContainer receiverDevices;
    receiverDevices = p2p.Install(routers.Get(1), receiver.Get(0));
    receiverDevices.add(p2p.Install(routers.Get(1), receiver.Get(1)));

    NetDeviceContainer background-trafficDevices;
    background-trafficDevices = p2p.Install(routers.Get(0), background-traffic1.Get(0));
    background-trafficDevices.add(p2p.Install(routers.Get(0), background-traffic1.Get(1)));
    background-trafficDevices.add(p2p.Install(routers.Get(1), background-traffic2.Get(0)));
    background-trafficDevices.add(p2p.Install(routers.Get(1), background-traffic2.Get(1)));

    NetDeviceContainer routerDevices;
    routerDevices = bottleneck.Install(routers.Get(0), routers.Get(1));

    IPv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer senderInterfaces = address.Assign(senderDevices);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer receiverInterfaces = address.Assign(receiverDevices);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer background-trafficInterfaces = address.Assign(background-trafficDevices);

    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer routerInterfaces = address.Assign(routerDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint64_t bdp = 400*1024*1024*50/(8*1000);
    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::TailDropQueueDisc", "MaxSize", QueueSizeValue(QueueSize(QueueSizeUnit::BYTES, bdp)));
    tch.install(routerDevices);

    uint port = 9;
    // Backgrounf Traffic from background-traffic1 to background-traffic3
    Address sinkAddress(InetSocketAddress(background-trafficDevices.Get(5), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(background-traffic2.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(600.0));

    OnOffHelper onOffHelper("ns3::TcpSocketFactory", sinkAddress);
    onOffHelper.SetAttribute("DataRate", DataRateValue(DataRate("100Kbps")));
    onOffHelper.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer onOffApp = onOffHelper.Install(background-traffic1.Get(0));
    onOffApp.Start(Seconds(1.0));
    onOffApp.Stop(Seconds(600.0));

    // Background Traffic from background-traffic4 to background-traffic2
    Address sinkAddress(InetSocketAddress(background-trafficDevices.Get(3), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(background-traffic2.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(600.0));

    OnOffHelper onOffHelper("ns3::TcpSocketFactory", sinkAddress);
    onOffHelper.SetAttribute("DataRate", DataRateValue(DataRate("100Kbps")));
    onOffHelper.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer onOffApp = onOffHelper.Install(background-traffic2.Get(1));
    onOffApp.Start(Seconds(1.0));
    onOffApp.Stop(Seconds(600.0));

    // Two CUBIC flows from sender to receiver one starting little later
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpCubic"));
    Address sinkAddress(InetSocketAddress(receiverInterfaces.GetAddress(1), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(receiver.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(600.0));

    OnOffHelper onOffHelper("ns3::TcpSocketFactory", sinkAddress);
    onOffHelper.SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    onOffHelper.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer onOffApp = onOffHelper.Install(sender.Get(0));
    onOffApp.Start(Seconds(0.0));
    onOffApp.Stop(Seconds(600.0));

    Address sinkAddress(InetSocketAddress(receiverInterfaces.GetAddress(3), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(receiver.Get(1));
    sinkApp.Start(Seconds(100.0));
    sinkApp.Stop(Seconds(600.0));

    OnOffHelper onOffHelper("ns3::TcpSocketFactory", sinkAddress);
    onOffHelper.SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    onOffHelper.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer onOffApp = onOffHelper.Install(sender.Get(1));
    onOffApp.Start(Seconds(100.0));
    onOffApp.Stop(Seconds(600.0));

    // Monitor the congestion window and throughput of both the flows continuously for 600 seconds using ascii trace files
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream1 = ascii.CreateFileStream("cwnd1.tr");
    Ptr<OutputStreamWrapper> stream2 = ascii.CreateFileStream("cwnd2.tr");
    sender.Get(0)->GetObject<TcpSocketBase>()->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback(&CwndChange, stream1));
    
    sender.Get(1)->GetObject<TcpSocketBase>()->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback(&CwndChange, stream2));

    Ptr<OutputStreamWrapper> stream3 = ascii.CreateFileStream("throughput1.tr");
    Ptr<OutputStreamWrapper> stream4 = ascii.CreateFileStream("throughput2.tr");

    Ptr<Node> node = sender.Get(0);
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    Ipv4InterfaceAddress iaddr = ipv4->GetAddress(1, 0);
    Ipv4Address sender1_addr = iaddr.GetLocal();

    Ptr<Node> node = sender.Get(1);
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    Ipv4InterfaceAddress iaddr = ipv4->GetAddress(1, 0);
    Ipv4Address sender2_addr = iaddr.GetLocal();


    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    Ptr<IPv4FlowClassifier> classifier = DynamicCast<IPv4FlowClassifier>(flowmon.GetClassifier());
    Simulator::Schedule(Seconds(1.0), &saveThroughput, monitor, classifier, throughput1, throughput2, sender1_addr, sender2_addr);
    
    Simulator::Stop(Seconds(600.0));
    Simulator::Run();
    monitor->CheckForLostPackets();
    Simulator::Destroy();

    return 0;
}