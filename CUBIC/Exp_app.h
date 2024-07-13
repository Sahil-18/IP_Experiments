#ifndef EXP_APP_H
#define EXP_APP_H

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

{
    class Application;

    class ExperimentApp : public Application
    {
        public:
            ExperimentApp();
            ~ExperimentApp() override;

            static TypeId GetTypeId(void);

            void Setup(Ptr<Socket> socket,
                       Address address,
                       uint32_t packetSize,
                       DataRate dataRate);
        private:
            void StartApplication() override;
            void StopApplication() override;

            void ScheduleTx();
            void SendPacket();

            Ptr<Socket> m_socket;
            Address m_peer;
            uint32_t m_packetSize;
            DataRate m_dataRate;
            EventId m_sendEvent;
            bool m_running;
    };
}

#endif