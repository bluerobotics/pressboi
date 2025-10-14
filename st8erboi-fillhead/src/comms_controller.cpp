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
			char errorMsg[] = "INJ_ERROR: RX QUEUE OVERFLOW - COMMAND DROPPED";
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
			char errorMsg[] = "INJ_ERROR: TX QUEUE OVERFLOW - MESSAGE DROPPED";
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

/**
 * This is the main parser that converts string commands from the network into
 * an enumerated type for easier handling within the state machine.
 * @param msg The raw string message from the network.
 * @return The `UserCommand` enum corresponding to the string, or `CMD_UNKNOWN`.
 */
Command CommsController::parseCommand(const char* msg) {
	// Trim leading whitespace
	while (isspace((unsigned char)*msg)) {
		msg++;
	}

	// If the message is empty after trimming, treat it as UNKNOWN and let the caller handle it.
	if (*msg == '\0') {
		return CMD_UNKNOWN;
	}

	// General Commands
	if (strncmp(msg, CMD_STR_DISCOVER_DEVICE, strlen(CMD_STR_DISCOVER_DEVICE)) == 0) return CMD_DISCOVER_DEVICE;
	if (strcmp(msg, CMD_STR_ENABLE) == 0) return CMD_ENABLE;
	if (strcmp(msg, CMD_STR_DISABLE) == 0) return CMD_DISABLE;
	if (strcmp(msg, CMD_STR_ABORT) == 0) return CMD_ABORT;
	if (strcmp(msg, CMD_STR_CLEAR_ERRORS) == 0) return CMD_CLEAR_ERRORS;

	// Injector Motion Commands
	if (strncmp(msg, CMD_STR_JOG_MOVE, strlen(CMD_STR_JOG_MOVE)) == 0) return CMD_JOG_MOVE;
	if (strcmp(msg, CMD_STR_MACHINE_HOME_MOVE) == 0) return CMD_MACHINE_HOME_MOVE;
	if (strcmp(msg, CMD_STR_CARTRIDGE_HOME_MOVE) == 0) return CMD_CARTRIDGE_HOME_MOVE;
	if (strncmp(msg, CMD_STR_INJECT_STATOR, strlen(CMD_STR_INJECT_STATOR)) == 0) return CMD_INJECT_STATOR;
	if (strncmp(msg, CMD_STR_INJECT_ROTOR, strlen(CMD_STR_INJECT_ROTOR)) == 0) return CMD_INJECT_ROTOR;
	if (strcmp(msg, CMD_STR_MOVE_TO_CARTRIDGE_HOME) == 0) return CMD_MOVE_TO_CARTRIDGE_HOME;
	if (strncmp(msg, CMD_STR_MOVE_TO_CARTRIDGE_RETRACT, strlen(CMD_STR_MOVE_TO_CARTRIDGE_RETRACT)) == 0) return CMD_MOVE_TO_CARTRIDGE_RETRACT;
	if (strcmp(msg, CMD_STR_PAUSE_INJECTION) == 0) return CMD_PAUSE_INJECTION;
	if (strcmp(msg, CMD_STR_RESUME_INJECTION) == 0) return CMD_RESUME_INJECTION;
	if (strcmp(msg, CMD_STR_CANCEL_INJECTION) == 0) return CMD_CANCEL_INJECTION;

	// Injection Valve Commands
	if (strcmp(msg, CMD_STR_INJECTION_VALVE_HOME_UNTUBED) == 0) return CMD_INJECTION_VALVE_HOME_UNTUBED;
	if (strcmp(msg, CMD_STR_INJECTION_VALVE_HOME_TUBED) == 0) return CMD_INJECTION_VALVE_HOME_TUBED;
	if (strcmp(msg, CMD_STR_INJECTION_VALVE_OPEN) == 0) return CMD_INJECTION_VALVE_OPEN;
	if (strcmp(msg, CMD_STR_INJECTION_VALVE_CLOSE) == 0) return CMD_INJECTION_VALVE_CLOSE;
	if (strncmp(msg, CMD_STR_INJECTION_VALVE_JOG, strlen(CMD_STR_INJECTION_VALVE_JOG)) == 0) return CMD_INJECTION_VALVE_JOG;

	// Vacuum Valve Commands
	if (strcmp(msg, CMD_STR_VACUUM_VALVE_HOME_UNTUBED) == 0) return CMD_VACUUM_VALVE_HOME_UNTUBED;
	if (strcmp(msg, CMD_STR_VACUUM_VALVE_HOME_TUBED) == 0) return CMD_VACUUM_VALVE_HOME_TUBED;
	if (strcmp(msg, CMD_STR_VACUUM_VALVE_OPEN) == 0) return CMD_VACUUM_VALVE_OPEN;
	if (strcmp(msg, CMD_STR_VACUUM_VALVE_CLOSE) == 0) return CMD_VACUUM_VALVE_CLOSE;
	if (strncmp(msg, CMD_STR_VACUUM_VALVE_JOG, strlen(CMD_STR_VACUUM_VALVE_JOG)) == 0) return CMD_VACUUM_VALVE_JOG;

	// Heater Commands
	if (strcmp(msg, CMD_STR_HEATER_ON) == 0) return CMD_HEATER_ON;
	if (strcmp(msg, CMD_STR_HEATER_OFF) == 0) return CMD_HEATER_OFF;
	if (strncmp(msg, CMD_STR_SET_HEATER_GAINS, strlen(CMD_STR_SET_HEATER_GAINS)) == 0) return CMD_SET_HEATER_GAINS;
	if (strncmp(msg, CMD_STR_SET_HEATER_SETPOINT, strlen(CMD_STR_SET_HEATER_SETPOINT)) == 0) return CMD_SET_HEATER_SETPOINT;

	// Vacuum Commands
	if (strcmp(msg, CMD_STR_VACUUM_ON) == 0) return CMD_VACUUM_ON;
	if (strcmp(msg, CMD_STR_VACUUM_OFF) == 0) return CMD_VACUUM_OFF;
	if (strcmp(msg, CMD_STR_VACUUM_LEAK_TEST) == 0) return CMD_VACUUM_LEAK_TEST;
	if (strncmp(msg, CMD_STR_SET_VACUUM_TARGET, strlen(CMD_STR_SET_VACUUM_TARGET)) == 0) return CMD_SET_VACUUM_TARGET;
	if (strncmp(msg, CMD_STR_SET_VACUUM_TIMEOUT_S, strlen(CMD_STR_SET_VACUUM_TIMEOUT_S)) == 0) return CMD_SET_VACUUM_TIMEOUT_S;
	if (strncmp(msg, CMD_STR_SET_LEAK_TEST_DELTA, strlen(CMD_STR_SET_LEAK_TEST_DELTA)) == 0) return CMD_SET_LEAK_TEST_DELTA;
	if (strncmp(msg, CMD_STR_SET_LEAK_TEST_DURATION_S, strlen(CMD_STR_SET_LEAK_TEST_DURATION_S)) == 0) return CMD_SET_LEAK_TEST_DURATION_S;

	return CMD_UNKNOWN;
}
