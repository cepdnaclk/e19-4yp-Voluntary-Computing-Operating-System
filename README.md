___
#  A Lightweight Operating System for Voluntary Computing
___

## Overview
VoluntaryOS is an experimental, lightweight operating system designed to explore and evaluate the feasibility of kernel-level support for voluntary computing. The project investigates how decentralized resource sharing can be improved by embedding voluntary computing primitives directly within the operating system architecture.

In contrast to application-level or browser-based voluntary computing systems (e.g., BOINC or Pando), VoluntaryOS integrates mechanisms for dynamic role negotiation, task scheduling, and secure resource isolation natively into the kernel and core system services. This design enables seamless transitions between controller (task distributor) and worker (task executor) roles based on device workload and availability, offering the potential for reduced latency, increased resource efficiency, and improved user control.

By testing this architecture across a network of heterogeneous devices, the project aims to:

1.Assess performance, scalability, and adaptability in real-time local network environments.

2.Identify key trade-offs between security, isolation, and communication overhead.

3.Contribute to the broader research domain of distributed systems and cooperative computing by providing a novel, OS-native implementation model.

## Key Features
Dynamic Role Assignment: Devices autonomously decide to become a controller or a worker based on CPU usage and task demand.

Local Network Discovery: Devices communicate over the local network using secure and efficient messaging protocols.

Sandboxed Execution: Tasks are executed in isolated environments to ensure security and resource control.

Modular Architecture: Pluggable components allow easy integration of scheduling, security, and resource management modules.

## Functional Components
### Role Manager

Monitors CPU and system usage.

Switches device role dynamically:

  High CPU Load: Act as Controller.

  Low CPU Load + Incoming Recruit Request: Act as Worker.

### Voluntary Compute Controller (VCC)

Accepts heavy workloads from local applications.

Broadcasts requests to recruit available workers.

Distributes tasks and aggregates results.

### Voluntary Compute Worker (VCW)

Responds to controller recruitment messages.

Accepts and executes tasks in secure sandboxes.

Returns results and availability status.

### Communication Protocol

Lightweight communication using WebSockets or custom UDP-based protocol.

Supports discovery, recruitment, task transfer, and status reporting.
