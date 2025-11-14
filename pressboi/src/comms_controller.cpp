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
	processUsbSerial();
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

void CommsController::processUsbSerial() {
	static char usbBuffer[MAX_MESSAGE_LENGTH];
	static int usbBufferIndex = 0;
	
	// Limit characters processed per call to prevent watchdog timeout (128ms)
	int charsProcessed = 0;
	const int MAX_CHARS_PER_CALL = 32;
	
	while (ConnectorUsb.AvailableForRead() > 0 && charsProcessed < MAX_CHARS_PER_CALL) {
		char c = ConnectorUsb.CharGet();
		charsProcessed++;
		
		// Handle newline as message terminator
		if (c == '\n' || c == '\r') {
			if (usbBufferIndex > 0) {
				usbBuffer[usbBufferIndex] = '\0';
				// Enqueue as if from local host (use dummy IP)
				IpAddress dummyIp(127, 0, 0, 1);
				if (!enqueueRx(usbBuffer, dummyIp, CLIENT_PORT)) {
					// Error handled in enqueueRx
				}
				usbBufferIndex = 0;
			}
		}
		// Add character to buffer
		else if (usbBufferIndex < MAX_MESSAGE_LENGTH - 1) {
			usbBuffer[usbBufferIndex++] = c;
		}
		// Buffer overflow protection - discard message
		else {
			usbBufferIndex = 0;
			ConnectorUsb.Send("PRESSBOI_ERROR: USB command too long\n");
		}
	}
}

void CommsController::processTxQueue() {
	if (m_txQueueHead != m_txQueueTail) {
		Message msg = m_txQueue[m_txQueueTail];
		m_txQueueTail = (m_txQueueTail + 1) % TX_QUEUE_SIZE;
		
		// Send over UDP
		m_udp.Connect(msg.remoteIp, msg.remotePort);
		m_udp.PacketWrite(msg.buffer);
		m_udp.PacketSend();
		
		// Mirror to USB serial
		ConnectorUsb.Send(msg.buffer);
		ConnectorUsb.Send("\n");
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
	// USB setup is non-blocking - the connector will become available when ready
	// No need to wait here, as the main loop will handle USB when available
}

void CommsController::setupEthernet() {
	EthernetMgr.Setup();

	// Start DHCP but don't hang if it fails - the system can still function via USB
	if (!EthernetMgr.DhcpBegin()) {
		// DHCP failed - continue anyway, network features won't work but USB will
		return;
	}
	
	// Wait for link with SHORT timeout to prevent watchdog timeout (128ms limit)
	uint32_t link_timeout = 100;  // 100ms timeout (well under 128ms watchdog)
	uint32_t link_start = Milliseconds();
	while (!EthernetMgr.PhyLinkActive()) {
		if (Milliseconds() - link_start > link_timeout) {
			// Link didn't come up quickly - continue anyway, USB will still work
			// Network may become available later
			return;
		}
		Delay_ms(5);  // Very short delay to stay under watchdog timeout
	}

	m_udp.Begin(LOCAL_PORT);
}

// parseCommand is now in commands.cpp as a global function
