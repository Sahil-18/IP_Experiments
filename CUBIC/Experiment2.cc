#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/stats-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("CubicExperiment");

void MeasureThroughput (Ptr<FlowMonitor> flowMonitor, Ptr<Ipv4FlowClassifier> classifier, double &throughput1, double &throughput2) {
    throughput1 = 0.0;
    throughput2 = 0.0;
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
        std::cout<<"Flow ID: "<<i->first<<" Source IP: "<<t.sourceAddress<<" Source Port: "<<t.sourcePort<<" Destination IP: "<<t.destinationAddress<<" Destination Port: "<<t.destinationPort<<"\n";
        if (i->first == 1) { // flow 1
            throughput1 = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1024 / 1024;
        } else if (i->first == 2) { // flow 2
            throughput2 = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1024 / 1024;
        }
    }
}

void RunExperiment (std::string tcpVariant, uint32_t rtt, double &throughput1, double &throughput2) {
    Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue (tcpVariant));

    NodeContainer routers, sender, receiver;
    sender.Create (2);
    routers.Create (2);
    receiver.Create (2);

    InternetStackHelper internet;
    internet.Install (routers);
    internet.Install (sender);
    internet.Install (receiver);
    uint32_t rtt_ = (rtt - 10)/4;

    PointToPointHelper p2p, bottleneck;
    p2p.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
    p2p.SetChannelAttribute ("Delay", StringValue (std::to_string(rtt_) + "ms"));

    uint32_t bdp = 400 /8 * 1024 * 1024 /1000 * 5;
    bottleneck.SetDeviceAttribute ("DataRate", StringValue ("400Mbps"));
    bottleneck.SetChannelAttribute ("Delay", StringValue ("5ms"));
    bottleneck.SetQueue ("ns3::DropTailQueue", "MaxSize", QueueSizeValue (QueueSize (QueueSizeUnit::BYTES, bdp)));

    NetDeviceContainer senderDevices1 = p2p.Install (sender.Get (0), routers.Get (0));
    NetDeviceContainer senderDevices2 = p2p.Install (sender.Get (1), routers.Get (0));

    NetDeviceContainer receiverDevices1 = p2p.Install (receiver.Get (0), routers.Get (1));
    NetDeviceContainer receiverDevices2 = p2p.Install (receiver.Get (1), routers.Get (1));

    NetDeviceContainer routerDevices = bottleneck.Install (routers.Get (0), routers.Get (1));

    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer senderInterfaces1 = address.Assign (senderDevices1);

    address.SetBase ("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer senderInterfaces2 = address.Assign (senderDevices2);

    address.SetBase ("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer receiverInterfaces1 = address.Assign (receiverDevices1);

    address.SetBase ("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer receiverInterfaces2 = address.Assign (receiverDevices2);

    address.SetBase ("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer routerInterfaces = address.Assign (routerDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    uint16_t port = 9;
    double START_TIME = 1.0;
    double STOP_TIME = 100.0;

    OnOffHelper onOffHelper1 ("ns3::TcpSocketFactory", InetSocketAddress (receiverInterfaces1.GetAddress (0), port));
    onOffHelper1.SetAttribute ("DataRate", DataRateValue (DataRate ("1Gbps")));
    onOffHelper1.SetAttribute ("PacketSize", UintegerValue (1024));
    onOffHelper1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer onOffApp1 = onOffHelper1.Install (sender.Get (0));
    onOffApp1.Start (Seconds (START_TIME));
    onOffApp1.Stop (Seconds (STOP_TIME));

    PacketSinkHelper sinkHelper1 ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
    ApplicationContainer sinkApp1 = sinkHelper1.Install (receiver.Get (0));
    sinkApp1.Start (Seconds (START_TIME));
    sinkApp1.Stop (Seconds (STOP_TIME));

    OnOffHelper onOffHelper2 ("ns3::TcpSocketFactory", InetSocketAddress (receiverInterfaces2.GetAddress (0), port));
    onOffHelper2.SetAttribute ("DataRate", DataRateValue (DataRate ("1Gbps")));
    onOffHelper2.SetAttribute ("PacketSize", UintegerValue (1024));
    onOffHelper2.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper2.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer onOffApp2 = onOffHelper2.Install (sender.Get (1));
    onOffApp2.Start (Seconds (START_TIME));
    onOffApp2.Stop (Seconds (STOP_TIME));

    PacketSinkHelper sinkHelper2 ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
    ApplicationContainer sinkApp2 = sinkHelper2.Install (receiver.Get (1));
    sinkApp2.Start (Seconds (START_TIME));
    sinkApp2.Stop (Seconds (STOP_TIME));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll ();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());

    Simulator::Stop (Seconds (STOP_TIME));
    std::cout<<"Starting simulation\n";
    Simulator::Run ();
    std::cout<<"Simulation completed\n";
    MeasureThroughput (monitor, classifier, throughput1, throughput2);

    Simulator::Destroy ();
}

int main (int argc, char *argv[]) {
    std::vector<uint32_t> rtts = {16, 32, 64, 128, 256, 512};
    std::vector<std::string> tcpVariants = {"ns3::TcpCubic", "ns3::TcpNewReno", "ns3::TcpBic", "ns3::TcpHighSpeed"};

    std::ofstream outFile;
    outFile.open ("tcp_fairness.csv");
    outFile << "TCP_Variant,RTT,Throughput1,Throughput2,Throughput_Ratio\n";

    for (std::string tcpVariant : tcpVariants) {
        for (uint32_t rtt : rtts) {
            double throughput1, throughput2;
            RunExperiment (tcpVariant, rtt, throughput1, throughput2);
            double throughputRatio = throughput1 / throughput2;
            outFile << tcpVariant << "," << rtt << "," << throughput1 << "," << throughput2 << "," << throughputRatio << "\n";
        }
    }

    outFile.close ();
    return 0;
}
