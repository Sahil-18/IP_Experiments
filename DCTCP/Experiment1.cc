/*
This is the first experiment of the DCTCP lab. In this experiment, 
we will simulate a simple network topology with 2 senders, 1 switch, 
and 1 receiver. The senders will send data to the receiver using DCTCP. 
We will use a RED queue disc for the switch. We will also monitor the 
queue size of the switch.
*/

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

std::ofstream queueSizes;

void CheckQueueSize(Ptr<QueueDisc> qdisc){
    uint32_t qSize = qdisc->GetNPackets();
    queueSizes<<Simulator::Now().GetSeconds()<<" "<<qSize<<std::endl;
    Simulator::Schedule(MilliSeconds(125), &CheckQueueSize, qdisc);
}

int main(){
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpDctcp"));
    NodeContainer nodes;
    // 2 Sender, 1 Switch, 1 Receiver
    nodes.Create(4);

    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));
    Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(2));
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));

    Config::SetDefault("ns3::RedQueueDisc::UseEcn", BooleanValue(true));
    Config::SetDefault("ns3::RedQueueDisc::UseHardDrop", BooleanValue(false));
    Config::SetDefault ("ns3::RedQueueDisc::MeanPktSize", UintegerValue (1500));
    Config::SetDefault("ns3::RedQueueDisc::MaxSize", QueueSizeValue(QueueSize("2666p")));
    Config::SetDefault ("ns3::RedQueueDisc::QW", DoubleValue (1));
    Config::SetDefault ("ns3::RedQueueDisc::MinTh", DoubleValue (10));
    Config::SetDefault ("ns3::RedQueueDisc::MaxTh", DoubleValue (20));

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    p2p.SetChannelAttribute("Delay", StringValue("10us"));

    NetDeviceContainer s1t1, s2t1, t2r;
    s1t1 = p2p.Install(nodes.Get(0), nodes.Get(2));
    s2t1 = p2p.Install(nodes.Get(1), nodes.Get(2));
    t2r = p2p.Install(nodes.Get(2), nodes.Get(3));

    InternetStackHelper stack;
    stack.Install(nodes);

    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::RedQueueDisc",
                         "LinkBandwidth", StringValue("1Gbps"),
                         "LinkDelay", StringValue("10us"),
                         "MinTh", DoubleValue(50),
                         "MaxTh", DoubleValue(80));

    QueueDiscContainer qdiscs = tch.Install(t2r);
    QueueDiscContainer qdiscs2 = tch.Install(s1t1);
    QueueDiscContainer qdiscs3 = tch.Install(s2t1);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer s1t1_interface = address.Assign(s1t1);
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer s2t1_interface = address.Assign(s2t1);
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer t2r_interface = address.Assign(t2r);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;
    OnOffHelper onOffHelper1("ns3::TcpSocketFactory", InetSocketAddress(t2r_interface.GetAddress(1), port));
    onOffHelper1.SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    onOffHelper1.SetAttribute("PacketSize", UintegerValue(1000));
    onOffHelper1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer sender1 = onOffHelper1.Install(nodes.Get(0));
    sender1.Start(Seconds(1.0));
    sender1.Stop(Seconds(5.0));

    OnOffHelper onOffHelper2("ns3::TcpSocketFactory", InetSocketAddress(t2r_interface.GetAddress(1), port + 1));
    onOffHelper2.SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    onOffHelper2.SetAttribute("PacketSize", UintegerValue(1000));
    onOffHelper2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer sender2 = onOffHelper2.Install(nodes.Get(1));
    sender2.Start(Seconds(1.0));
    sender2.Stop(Seconds(5.0));

    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sink = sinkHelper.Install(nodes.Get(3));
    sink.Start(Seconds(1.0));
    sink.Stop(Seconds(5.0));

    PacketSinkHelper sinkHelper2("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port + 1));
    ApplicationContainer sink2 = sinkHelper2.Install(nodes.Get(3));
    sink2.Start(Seconds(1.0));
    sink2.Stop(Seconds(5.0));

    queueSizes.open("queueSizes.dat");
    queueSizes<<"Time(s) QueueSize(packets)"<<std::endl;

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Schedule(MilliSeconds(125), &CheckQueueSize, qdiscs.Get(0));

    Simulator::Stop(Seconds(5.0));
    Simulator::Run();

    Simulator::Destroy();
    // Save the monitor data
    monitor->SerializeToXmlFile("lab-4.flowmon", true, true);

    queueSizes.close();
}
