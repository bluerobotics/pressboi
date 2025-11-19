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
	
	// USB host health tracking - start pessimistic (wait for first sign of host)
	m_lastUsbHealthy = 0;
	m_usbHostConnected = false;
}

void CommsController::setup() {
    setupUsbSerial();
    setupEthernet();
}

void CommsController::update() {
	#if WATCHDOG_ENABLED
	g_watchdogBreadcrumb = WD_BREADCRUMB_UDP_PROCESS;
	#endif
	processUdp();
	
	#if WATCHDOG_ENABLED
	g_watchdogBreadcrumb = WD_BREADCRUMB_USB_PROCESS;
	#endif
	processUsbSerial();
	
	#if WATCHDOG_ENABLED
	g_watchdogBreadcrumb = WD_BREADCRUMB_TX_QUEUE;
	#endif
	processTxQueue();
}

bool CommsController::enqueueRx(const char* msg, const IpAddress& ip, uint16_t port) {
	int next_head = (m_rxQueueHead + 1) % RX_QUEUE_SIZE;
	if (next_head == m_rxQueueTail) {
		if(m_guiDiscovered && EthernetMgr.PhyLinkActive()) {
			char errorMsg[128];
			snprintf(errorMsg, sizeof(errorMsg), "%s_ERROR: RX QUEUE OVERFLOW - COMMAND DROPPED", DEVICE_NAME_UPPER);
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
		if(m_guiDiscovered && EthernetMgr.PhyLinkActive()) {
			char errorMsg[128];
			snprintf(errorMsg, sizeof(errorMsg), "%s_ERROR: TX QUEUE OVERFLOW - MESSAGE DROPPED", DEVICE_NAME_UPPER);
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
				
				// Mark USB host as active when we receive a command
				notifyUsbHostActive();
				
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
			char errorMsg[128];
			snprintf(errorMsg, sizeof(errorMsg), "%s_ERROR: USB command too long\n", DEVICE_NAME_UPPER);
			ConnectorUsb.Send(errorMsg);
		}
	}
}

void CommsController::processTxQueue() {
	// Process one message per call - this is fine since watchdog is fed every loop iteration
	if (m_txQueueHead != m_txQueueTail) {
		Message msg = m_txQueue[m_txQueueTail];
		m_txQueueTail = (m_txQueueTail + 1) % TX_QUEUE_SIZE;
		
		
		// Send over UDP if ethernet link is up AND we have a valid network GUI IP
		#if WATCHDOG_ENABLED
		g_watchdogBreadcrumb = WD_BREADCRUMB_UDP_SEND;
		#endif
		
		// Check if we have a valid network IP (not localhost, not 0.0.0.0)
		IpAddress localhost(127, 0, 0, 1);
		IpAddress zeroIp(0, 0, 0, 0);
		bool isLocalhost = (msg.remoteIp == localhost);
		bool isZero = (msg.remoteIp == zeroIp);
		bool hasValidNetworkIp = !isLocalhost && !isZero;
		
		if (EthernetMgr.PhyLinkActive() && hasValidNetworkIp) {
			m_udp.Connect(msg.remoteIp, msg.remotePort);
			m_udp.PacketWrite(msg.buffer);
			m_udp.PacketSend();
		}
		// Note: If ethernet link is down or no valid network IP, UDP send is skipped
		// This prevents watchdog timeouts from blocking UDP sends
		
	// Mirror to USB serial (if USB host is connected and reading)
	// USB CDC buffer is 64 bytes, so we chunk messages larger than 50 bytes
	// 
	// USB Connection Detection:
	// - If USB buffer has space, a host is connected and reading data
	// - If USB buffer stays full for >3 seconds, no host is reading (disconnected or app closed)
	// - Stop sending to prevent buffer deadlock, resume automatically when host reconnects
	// - If host sends ANY data (commands), immediately mark as connected (via notifyUsbHostActive)
	
	uint32_t now = Milliseconds();
	int usbAvail = ConnectorUsb.AvailableForWrite();
	
	// Check if USB buffer has space - if yes, a host is actively reading
	if (usbAvail > 5) {
		m_lastUsbHealthy = now;
		if (!m_usbHostConnected) {
			// USB host reconnected! Resume sending
			m_usbHostConnected = true;
			// Send recovery message only if buffer has enough space (avoid blocking)
			if (usbAvail > 40) {
				char recoveryMsg[64];
				snprintf(recoveryMsg, sizeof(recoveryMsg), "%s_INFO: USB host reconnected\n", DEVICE_NAME_UPPER);
				ConnectorUsb.Send(recoveryMsg);
			}
		}
	} else {
		// Buffer is nearly full - either host is slow or disconnected
		if (m_usbHostConnected && (now - m_lastUsbHealthy) > 3000) {
			// Buffer full for 3+ seconds - host disconnected or stopped reading
			// Note: Can't send message here since buffer is full, but log state change internally
			m_usbHostConnected = false;
			// Stop sending to prevent buffer deadlock
		}
	}
	
	// Only send to USB if a host is connected and reading
	if (m_usbHostConnected) {
		const int CHUNK_SIZE = 50;
		int msgLen = strlen(msg.buffer);
		
		if (msgLen <= CHUNK_SIZE) {
			// Small message - send directly if buffer available
			if (usbAvail >= msgLen + 1) {
				ConnectorUsb.Send(msg.buffer);
				ConnectorUsb.Send("\n");
			}
			// If buffer full, just drop the message silently
		} else {
			// Large message - send in chunks with continuation markers
			// Format: "MSG_PART_1/3: first_50_chars"
			int totalChunks = (msgLen + CHUNK_SIZE - 1) / CHUNK_SIZE;
			for (int chunk = 0; chunk < totalChunks; chunk++) {
				int offset = chunk * CHUNK_SIZE;
				int chunkLen = (msgLen - offset > CHUNK_SIZE) ? CHUNK_SIZE : (msgLen - offset);
				
				char chunkMsg[80];
				snprintf(chunkMsg, sizeof(chunkMsg), "CHUNK_%d/%d:", chunk + 1, totalChunks);
				int headerLen = strlen(chunkMsg);
				
				// Add chunk data
				strncat(chunkMsg, msg.buffer + offset, chunkLen);
				chunkMsg[headerLen + chunkLen] = '\0';
				
				// Wait for buffer space (with timeout to prevent watchdog)
				uint32_t startWait = Milliseconds();
				while (ConnectorUsb.AvailableForWrite() < (int)strlen(chunkMsg) + 1) {
					if (Milliseconds() - startWait > 10) {
						// Timeout - skip this chunk to prevent blocking
						break;
					}
				}
				
				if (ConnectorUsb.AvailableForWrite() >= (int)strlen(chunkMsg) + 1) {
					ConnectorUsb.Send(chunkMsg);
					ConnectorUsb.Send("\n");
				}
			}
		}
	}
	// If USB unhealthy or buffer full, message is silently dropped to prevent blocking
	// USB will auto-recover when buffer has space again (host reconnects)
	}
}

void CommsController::reportEvent(const char* statusType, const char* message) {
	char fullMsg[MAX_MESSAGE_LENGTH];
	snprintf(fullMsg, sizeof(fullMsg), "%s%s", statusType, message);
	
	// Always queue messages for TX - they will be sent to both network (if GUI discovered) and USB
	// If GUI not discovered, use dummy IP - processTxQueue will still mirror to USB
	IpAddress targetIp = m_guiDiscovered ? m_guiIp : IpAddress(0, 0, 0, 0);
	uint16_t targetPort = m_guiDiscovered ? m_guiPort : 0;
	
	enqueueTx(fullMsg, targetIp, targetPort);
}

void CommsController::notifyUsbHostActive() {
	// Called when a command is received over USB
	// Immediately mark the host as connected and reset the health timer
	if (!m_usbHostConnected) {
		// Queue a message to indicate USB host was detected (don't send directly to avoid blocking)
		char msg[80];
		snprintf(msg, sizeof(msg), "%s_INFO: USB host detected via command", DEVICE_NAME_UPPER);
		// Use dummy IP for USB-originated message
		IpAddress dummyIp(127, 0, 0, 1);
		enqueueTx(msg, dummyIp, 0);
	}
	m_usbHostConnected = true;
	m_lastUsbHealthy = Milliseconds();
}

void CommsController::setupUsbSerial(void) {
	ConnectorUsb.Mode(Connector::USB_CDC);
	ConnectorUsb.Speed(9600);
	ConnectorUsb.PortOpen();
	// USB setup is non-blocking - the connector will become available when ready
	// No need to wait here, as the main loop will handle USB when available
	
	// Send a startup message after a delay to confirm USB is working
	// (Will be sent in the main loop once USB is ready)
}

void CommsController::setupEthernet() {
    EthernetMgr.Setup();

    // Start DHCP but don't hang if it fails - the system can still function via USB
    if (!EthernetMgr.DhcpBegin()) {
        // DHCP failed - continue anyway, network features won't work but USB will
        return;
    }

    // Wait for link with timeout (watchdog not yet enabled at this point)
    uint32_t link_timeout = 2000;  // 2 second timeout (plenty of time for link to come up)
    uint32_t link_start = Milliseconds();
    while (!EthernetMgr.PhyLinkActive()) {
        if (Milliseconds() - link_start > link_timeout) {
            // Link didn't come up - continue anyway, USB will still work
            // Network may become available later
            return;
        }
        Delay_ms(10);  // Short delay between checks
    }

    m_udp.Begin(LOCAL_PORT);
    
    // Send status message over USB to confirm network is ready
    char infoMsg[128];
    snprintf(infoMsg, sizeof(infoMsg), "%s_INFO: Network ready, listening on port %d\n", DEVICE_NAME_UPPER, LOCAL_PORT);
    ConnectorUsb.Send(infoMsg);
}

// parseCommand is now in commands.cpp as a global function
