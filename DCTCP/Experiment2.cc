/*
This is the second DCTCP experiment. In this experiment, we will simulate a simple network topology
of 6 devices connected to each other via a switch. We will use DCTCP to send data between the devices.
5 devices will send data to the 6th device. We will use a RED queue disc for the switch. The objective
of this experiment is to observer the convergence of DCTCP. Hence, each sender will start sending data 
at different times (10 seconds apart) and end at different times (10 seconds apart). We will monitor the
throughput of each sender and the queue size of the switch.
*/

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

#define START_TIME 0
#define END_TIME 100
#define JUMP 10
#define RESULT_TIME 0.1

std::ofstream queueSizes;
std::ofstream throughput;
std::map<FlowId, uint32_t> TotalRxBytes;

void CheckQueueSize(Ptr<QueueDisc> qdisc){
    std::cout<<"Progress "<<Simulator::Now().GetSeconds() <<" Seconds"<<std::endl;
    uint32_t qSize = qdisc->GetNPackets();
    queueSizes<<Simulator::Now().GetSeconds()<<"\t"<<qSize<<std::endl;
    Simulator::Schedule(Seconds(RESULT_TIME), &CheckQueueSize, qdisc);
}

void CheckThroughput(Ptr<FlowMonitor> monitor, Ptr<Ipv4FlowClassifier> classifier){
    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for(std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); i++){
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        if(t.destinationPort >= 9 && t.destinationPort <= 13){
            if(TotalRxBytes.find(i->first) == TotalRxBytes.end()){
                TotalRxBytes[i->first] = 0;
            }
            uint32_t lastTotalRxBytes = TotalRxBytes[i->first];
            uint32_t currentTotalRxBytes = i->second.rxBytes;
            double throughput_ = (currentTotalRxBytes - lastTotalRxBytes) * 8.0 / RESULT_TIME / 1024 / 1024;
            throughput<<Simulator::Now().GetSeconds()<<"\t"<<i->first<<"\t"<<throughput_<<std::endl;    
            TotalRxBytes[i->first] = currentTotalRxBytes;
        }
    }
    Simulator::Schedule(Seconds(RESULT_TIME), &CheckThroughput, monitor, classifier);
}

int main(){
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpDctcp"));
    NodeContainer nodes;
    // 6 devices connected to each other via a switch
    nodes.Create(6);
    Ptr<Node> T = CreateObject<Node>();
    
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));
    Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(2));
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));

    Config::SetDefault("ns3::RedQueueDisc::UseEcn", BooleanValue(true));
    Config::SetDefault("ns3::RedQueueDisc::UseHardDrop", BooleanValue(false));
    Config::SetDefault("ns3::RedQueueDisc::MeanPktSize", UintegerValue(1500));
    Config::SetDefault("ns3::RedQueueDisc::MaxSize", QueueSizeValue(QueueSize("2666p")));
    Config::SetDefault("ns3::RedQueueDisc::QW", DoubleValue(1));
    Config::SetDefault("ns3::RedQueueDisc::MinTh", DoubleValue(10));
    Config::SetDefault("ns3::RedQueueDisc::MaxTh", DoubleValue(20));

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    p2p.SetChannelAttribute("Delay", StringValue("10us"));

    std::vector<NetDeviceContainer> devices;
    for(uint32_t i = 0; i < 6; i++){
        NetDeviceContainer s1t1 = p2p.Install(nodes.Get(i), T);
        devices.push_back(s1t1);
    }

    InternetStackHelper stack;
    stack.InstallAll();

    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::RedQueueDisc",
                         "LinkBandwidth", StringValue("1Gbps"),
                         "LinkDelay", StringValue("10us"),
                         "LinkBandwidth", StringValue("1Gbps"),
                         "LinkDelay", StringValue("10us"),
                         "MinTh", DoubleValue(10),
                         "MaxTh", DoubleValue(20));

    std::vector<QueueDiscContainer> qdiscs;
    for(uint32_t i = 0; i < 6; i++){
        QueueDiscContainer qdisc = tch.Install(devices[i]);
        qdiscs.push_back(qdisc);
    }

    std::vector<Ipv4InterfaceContainer> interfaces;
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    for(uint32_t i = 0; i < 6; i++){
        Ipv4InterfaceContainer interface = address.Assign(devices[i]);
        interfaces.push_back(interface);
        address.NewNetwork();
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Create 5 senders
    std::vector<ApplicationContainer> sendApp;
    std::vector<ApplicationContainer> receiveApp;

    uint16_t port = 9;
    for(uint32_t i = 0; i < 5; i++){
        OnOffHelper onOffHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces[5].GetAddress(0), port + i));
        onOffHelper.SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
        onOffHelper.SetAttribute("PacketSize", UintegerValue(1000));
        onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

        ApplicationContainer sender = onOffHelper.Install(nodes.Get(i));
        sendApp.push_back(sender);
        sender.Start(Seconds(START_TIME + i * JUMP));
        sender.Stop(Seconds(END_TIME - i * JUMP));

        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port + i));
        ApplicationContainer sink = sinkHelper.Install(nodes.Get(5));
        receiveApp.push_back(sink);
        sink.Start(Seconds(START_TIME + i * JUMP));
        sink.Stop(Seconds(END_TIME - i * JUMP));
    }

    queueSizes.open("queue_sizes.dat");
    queueSizes << "Time(s)\tQueueSize(packets)"<<std::endl;

    throughput.open("throughput.dat");
    throughput << "Time(s)\tFlow ID\tThroughput(Mbps)"<<std::endl;

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());

    Simulator::Schedule(Seconds(RESULT_TIME), &CheckQueueSize, qdiscs[5].Get(1));
    Simulator::Schedule(Seconds(RESULT_TIME), &CheckThroughput, monitor, classifier);

    Simulator::Stop(Seconds(END_TIME));
    Simulator::Run();

    Simulator::Destroy();

    queueSizes.close();
    throughput.close();
}