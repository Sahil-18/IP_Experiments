/*
This is the third CUBIC experiment. Here, we will measure the throughput 
of four TCP flows of same TCP variant (CUBIC, DCTCP, HSTCP, TCP New RENO) 
competing against 4 TCP flows of TCP RENO variant. We will vary the RTT 
from 10ms to 160ms and measure the throughput of each flow.
*/

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
    // Here we have to accumulate the throughput from port 9 to one variable and from port 10 to another variable. Add the throughput of all flows to the respective variables.
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
        std::cout<<"Flow ID: "<<i->first<<" Source IP: "<<t.sourceAddress<<" Source Port: "<<t.sourcePort<<" Destination IP: "<<t.destinationAddress<<" Destination Port: "<<t.destinationPort<<"\n";
        if (t.destinationPort == 9) { // flow 1
            throughput1 += i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1024 / 1024;
        } else if (t.destinationPort == 10) { // flow 2
            throughput2 += i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1024 / 1024;
        }
    }
}

void RunExperiment (std::string tcpVariant, uint32_t rtt, double &throughput1, double &throughput2) {
    NodeContainer routers, sender, receiver;
    sender.Create (4);
    routers.Create (2);
    receiver.Create (4);

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

    NetDeviceContainer senderDevices[4], receiverDevices[4], routerDevices; 
    for (uint32_t i = 0; i < 4; i++) {
        senderDevices[i] = p2p.Install (sender.Get (i), routers.Get (0));
        receiverDevices[i] = p2p.Install (receiver.Get (i), routers.Get (1));
    }

    routerDevices = bottleneck.Install (routers.Get (0), routers.Get (1));

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer senderInterfaces[4], receiverInterfaces[4], routerInterfaces;
    for (uint32_t i = 0; i < 4; i++) {
        std::string add = "10.1." + std::to_string(i+1) + ".0";
        address.SetBase ( add.c_str() ,"255.255.255.255");
        senderInterfaces[i] = address.Assign (senderDevices[i]);
        std::string add2 = "10.2." + std::to_string(i+1) + ".0";
        address.SetBase ( add2.c_str(), "255.255.255.255");
        receiverInterfaces[i] = address.Assign (receiverDevices[i]);
    }
    address.SetBase ("10.3.1.0", "255.255.255.255");
    routerInterfaces = address.Assign (routerDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    uint16_t port = 9, port2 = 10;
    double START_TIME = 1.0;
    double STOP_TIME = 10.0;

    std::cout<<"Creating applications\n";
    // Default TCP apps
    Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));
    // App 1
    OnOffHelper onOffHelperReno1 ("ns3::TcpSocketFactory", InetSocketAddress (receiverInterfaces[0].GetAddress (0), port));
    onOffHelperReno1.SetAttribute ("DataRate", DataRateValue (DataRate ("1Gbps")));
    onOffHelperReno1.SetAttribute ("PacketSize", UintegerValue (1024));
    onOffHelperReno1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelperReno1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer onOffAppReno1 = onOffHelperReno1.Install (sender.Get (0));
    onOffAppReno1.Start (Seconds (START_TIME));
    onOffAppReno1.Stop (Seconds (STOP_TIME));

    PacketSinkHelper sinkHelperReno1 ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
    ApplicationContainer sinkAppReno1 = sinkHelperReno1.Install (receiver.Get (0));
    sinkAppReno1.Start (Seconds (START_TIME));
    sinkAppReno1.Stop (Seconds (STOP_TIME));

    // App 2
    OnOffHelper onOffHelperReno2 ("ns3::TcpSocketFactory", InetSocketAddress (receiverInterfaces[1].GetAddress (0), port));
    onOffHelperReno2.SetAttribute ("DataRate", DataRateValue (DataRate ("1Gbps")));
    onOffHelperReno2.SetAttribute ("PacketSize", UintegerValue (1024));
    onOffHelperReno2.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelperReno2.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer onOffAppReno2 = onOffHelperReno2.Install (sender.Get (1));
    onOffAppReno2.Start (Seconds (START_TIME));
    onOffAppReno2.Stop (Seconds (STOP_TIME));

    PacketSinkHelper sinkHelperReno2 ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
    ApplicationContainer sinkAppReno2 = sinkHelperReno1.Install (receiver.Get (1));
    sinkAppReno2.Start (Seconds (START_TIME));
    sinkAppReno2.Stop (Seconds (STOP_TIME));

    // App3
    OnOffHelper onOffHelperReno3 ("ns3::TcpSocketFactory", InetSocketAddress (receiverInterfaces[2].GetAddress (0), port));
    onOffHelperReno3.SetAttribute ("DataRate", DataRateValue (DataRate ("1Gbps")));
    onOffHelperReno3.SetAttribute ("PacketSize", UintegerValue (1024));
    onOffHelperReno3.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelperReno3.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer onOffAppReno3 = onOffHelperReno3.Install (sender.Get (2));
    onOffAppReno3.Start (Seconds (START_TIME));
    onOffAppReno3.Stop (Seconds (STOP_TIME));

    PacketSinkHelper sinkHelperReno3 ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
    ApplicationContainer sinkAppReno3 = sinkHelperReno3.Install (receiver.Get (2));
    sinkAppReno3.Start (Seconds (START_TIME));
    sinkAppReno3.Stop (Seconds (STOP_TIME));

    // App4
    OnOffHelper onOffHelperReno4 ("ns3::TcpSocketFactory", InetSocketAddress (receiverInterfaces[3].GetAddress (0), port));
    onOffHelperReno4.SetAttribute ("DataRate", DataRateValue (DataRate ("1Gbps")));
    onOffHelperReno4.SetAttribute ("PacketSize", UintegerValue (1024));
    onOffHelperReno4.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelperReno4.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer onOffAppReno4 = onOffHelperReno4.Install (sender.Get (3));
    onOffAppReno4.Start (Seconds (START_TIME));
    onOffAppReno4.Stop (Seconds (STOP_TIME));

    PacketSinkHelper sinkHelperReno4 ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
    ApplicationContainer sinkAppReno4 = sinkHelperReno4.Install (receiver.Get (3));
    sinkAppReno4.Start (Seconds (START_TIME));
    sinkAppReno4.Stop (Seconds (STOP_TIME));

    std::cout<<"Creating other applications\n";

    // Variant TCP apps
    Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue (tcpVariant));
    // App 1
    OnOffHelper onOffHelperVariant1 ("ns3::TcpSocketFactory", InetSocketAddress (receiverInterfaces[0].GetAddress (0), port2));
    onOffHelperVariant1.SetAttribute ("DataRate", DataRateValue (DataRate ("1Gbps")));
    onOffHelperVariant1.SetAttribute ("PacketSize", UintegerValue (1024));
    onOffHelperVariant1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelperVariant1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer onOffAppVariant1 = onOffHelperVariant1.Install (sender.Get (0));
    onOffAppVariant1.Start (Seconds (START_TIME));
    onOffAppVariant1.Stop (Seconds (STOP_TIME));

    PacketSinkHelper sinkHelperVariant1 ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port2));
    ApplicationContainer sinkAppVariant1 = sinkHelperVariant1.Install (receiver.Get (0));
    sinkAppVariant1.Start (Seconds (START_TIME));
    sinkAppVariant1.Stop (Seconds (STOP_TIME));

    // App 2
    OnOffHelper onOffHelperVariant2 ("ns3::TcpSocketFactory", InetSocketAddress (receiverInterfaces[1].GetAddress (0), port2));
    onOffHelperVariant2.SetAttribute ("DataRate", DataRateValue (DataRate ("1Gbps")));
    onOffHelperVariant2.SetAttribute ("PacketSize", UintegerValue (1024));
    onOffHelperVariant2.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelperVariant2.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer onOffAppVariant2 = onOffHelperVariant2.Install (sender.Get (1));
    onOffAppVariant2.Start (Seconds (START_TIME));
    onOffAppVariant2.Stop (Seconds (STOP_TIME));

    PacketSinkHelper sinkHelperVariant2 ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port2));
    ApplicationContainer sinkAppVariant2 = sinkHelperVariant2.Install (receiver.Get (1));
    sinkAppVariant2.Start (Seconds (START_TIME));
    sinkAppVariant2.Stop (Seconds (STOP_TIME));

    // App3
    OnOffHelper onOffHelperVariant3 ("ns3::TcpSocketFactory", InetSocketAddress (receiverInterfaces[2].GetAddress (0), port2));
    onOffHelperVariant3.SetAttribute ("DataRate", DataRateValue (DataRate ("1Gbps")));
    onOffHelperVariant3.SetAttribute ("PacketSize", UintegerValue (1024));
    onOffHelperVariant3.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelperVariant3.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer onOffAppVariant3 = onOffHelperVariant3.Install (sender.Get (2));
    onOffAppVariant3.Start (Seconds (START_TIME));
    onOffAppVariant3.Stop (Seconds (STOP_TIME));

    PacketSinkHelper sinkHelperVariant3 ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port2));
    ApplicationContainer sinkAppVariant3 = sinkHelperVariant3.Install (receiver.Get (2));
    sinkAppVariant3.Start (Seconds (START_TIME));
    sinkAppVariant3.Stop (Seconds (STOP_TIME));

    // App4
    OnOffHelper onOffHelperVariant4 ("ns3::TcpSocketFactory", InetSocketAddress (receiverInterfaces[3].GetAddress (0), port2));
    onOffHelperVariant4.SetAttribute ("DataRate", DataRateValue (DataRate ("1Gbps")));
    onOffHelperVariant4.SetAttribute ("PacketSize", UintegerValue (1024));
    onOffHelperVariant4.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelperVariant4.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer onOffAppVariant4 = onOffHelperVariant4.Install (sender.Get (3));
    onOffAppVariant4.Start (Seconds (START_TIME));
    onOffAppVariant4.Stop (Seconds (STOP_TIME));

    PacketSinkHelper sinkHelperVariant4 ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port2));
    ApplicationContainer sinkAppVariant4 = sinkHelperVariant4.Install (receiver.Get (3));
    sinkAppVariant4.Start (Seconds (START_TIME));
    sinkAppVariant4.Stop (Seconds (STOP_TIME));


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
    std::vector<uint32_t> rtts = {10, 40, 80, 120, 160};
    std::vector<std::string> tcpVariants = {"ns3::TcpCubic", "ns3::TcpNewReno", "ns3::TcpBic", "ns3::TcpHighSpeed"};

    std::ofstream outFile;
    outFile.open ("tcp_friendliness.csv");
    outFile << "TCP_Variant,RTT,Throughput1,Throughput2,\n";

    for (std::string tcpVariant : tcpVariants) {
        for (uint32_t rtt : rtts) {
            double throughput1, throughput2;
            RunExperiment (tcpVariant, rtt, throughput1, throughput2);
           outFile << tcpVariant << "," << rtt << "," << throughput1 << "," << throughput2 << "\n";
        }

    }
    outFile.close ();
    return 0;
}
