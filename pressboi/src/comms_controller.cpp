/**
 * @file comms_controller.cpp
 * @author Eldin Miller-Stead
 * @date September 10, 2025
 * @brief Implements the CommsController for handling network and serial communications.
 *
 * @details This file provides the concrete implementation for the methods declared in
 * `comms_controller.h`. It includes the logic for managing the circular message queues,
 * processing UDP packets, and parsing command strings.
 */
#include "comms_controller.h"

CommsController::CommsController() {
	m_guiDiscovered = false;
	m_guiPort = 0;
	
	m_rxQueueHead = 0;
	m_rxQueueTail = 0;
	m_txQueueHead = 0;
	m_txQueueTail = 0;
}

void CommsController::setup() {
	setupUsbSerial();
	setupEthernet();
}

void CommsController::update() {
	processUdp();
	processTxQueue();
}

bool CommsController::enqueueRx(const char* msg, const IpAddress& ip, uint16_t port) {
	int next_head = (m_rxQueueHead + 1) % RX_QUEUE_SIZE;
	if (next_head == m_rxQueueTail) {
		if(m_guiDiscovered) {
			char errorMsg[] = "PRESSBOI_ERROR: RX QUEUE OVERFLOW - COMMAND DROPPED";
			m_udp.Connect(m_guiIp, m_guiPort);
			m_udp.PacketWrite(errorMsg);
			m_udp.PacketSend();
		}
		return false;
	}
	strncpy(m_rxQueue[m_rxQueueHead].buffer, msg, MAX_MESSAGE_LENGTH);
	m_rxQueue[m_rxQueueHead].buffer[MAX_MESSAGE_LENGTH - 1] = '\0';
	m_rxQueue[m_rxQueueHead].remoteIp = ip;
	m_rxQueue[m_rxQueueHead].remotePort = port;
	m_rxQueueHead = next_head;
	return true;
}

bool CommsController::dequeueRx(Message& msg) {
	if (m_rxQueueHead == m_rxQueueTail) {
		return false;
	}
	msg = m_rxQueue[m_rxQueueTail];
	m_rxQueueTail = (m_rxQueueTail + 1) % RX_QUEUE_SIZE;
	return true;
}

bool CommsController::enqueueTx(const char* msg, const IpAddress& ip, uint16_t port) {
	int next_head = (m_txQueueHead + 1) % TX_QUEUE_SIZE;
	if (next_head == m_txQueueTail) {
		if(m_guiDiscovered) {
			char errorMsg[] = "PRESSBOI_ERROR: TX QUEUE OVERFLOW - MESSAGE DROPPED";
			m_udp.Connect(m_guiIp, m_guiPort);
			m_udp.PacketWrite(errorMsg);
			m_udp.PacketSend();
		}
		return false;
	}
	strncpy(m_txQueue[m_txQueueHead].buffer, msg, MAX_MESSAGE_LENGTH);
	m_txQueue[m_txQueueHead].buffer[MAX_MESSAGE_LENGTH - 1] = '\0';
	m_txQueue[m_txQueueHead].remoteIp = ip;
	m_txQueue[m_txQueueHead].remotePort = port;
	m_txQueueHead = next_head;
	return true;
}

void CommsController::processUdp() {
	while (m_udp.PacketParse()) {
		IpAddress remoteIp = m_udp.RemoteIp();
		uint16_t remotePort = m_udp.RemotePort();
		int32_t bytesRead = m_udp.PacketRead(m_packetBuffer, MAX_PACKET_LENGTH - 1);
		if (bytesRead > 0) {
			m_packetBuffer[bytesRead] = '\0';
			if (!enqueueRx((char*)m_packetBuffer, remoteIp, remotePort)) {
				// Error handled in enqueueRx
			}
		}
	}
}

void CommsController::processTxQueue() {
	if (m_txQueueHead != m_txQueueTail) {
		Message msg = m_txQueue[m_txQueueTail];
		m_txQueueTail = (m_txQueueTail + 1) % TX_QUEUE_SIZE;
		m_udp.Connect(msg.remoteIp, msg.remotePort);
		m_udp.PacketWrite(msg.buffer);
		m_udp.PacketSend();
	}
}

void CommsController::reportEvent(const char* statusType, const char* message) {
	char fullMsg[MAX_MESSAGE_LENGTH];
	snprintf(fullMsg, sizeof(fullMsg), "%s%s", statusType, message);
	
	if (m_guiDiscovered) {
		enqueueTx(fullMsg, m_guiIp, m_guiPort);
	}
}

void CommsController::setupUsbSerial(void) {
	ConnectorUsb.Mode(Connector::USB_CDC);
	ConnectorUsb.Speed(9600);
	ConnectorUsb.PortOpen();
	uint32_t timeout = 5000;
	uint32_t start = Milliseconds();
	while (!ConnectorUsb && Milliseconds() - start < timeout);
}

void CommsController::setupEthernet() {
	EthernetMgr.Setup();

	if (!EthernetMgr.DhcpBegin()) {
		while (1);
	}
	
	while (!EthernetMgr.PhyLinkActive()) {
		Delay_ms(100);
	}

	m_udp.Begin(LOCAL_PORT);
}

// parseCommand is now in commands.cpp as a global function
