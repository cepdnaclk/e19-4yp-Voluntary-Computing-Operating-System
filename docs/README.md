---
layout: home
permalink: index.html

# Please update this with your repository name and title
repository-name: e19-4yp-Voluntary-Computing-Operating-System
title: Designing a Kernel-Level Framework for Personal Voluntary Computing in Local Networks
---

[comment]: # "This is the standard layout for the project, but you can clean this and use your own template"

# Designing a Kernel-Level Framework for Personal Voluntary Computing in Local Networks

#### Team

- e19436, G.T.G. Wickramasingha, [email](e19436@eng.pdn.ac.lk)
- e19443, K.G.D.T. Wijayawardana, [email](e19443@eng.pdn.ac.lk)
- e19505, S.P.M. Witharana, [email](e19505@eng.pdn.ac.lk)

#### Supervisors

- Prof. Manjula Sandirigama, [email](manjula.sandirigama@eng.pdn.ac.lk)

#### Table of content

1. [Abstract](#abstract)
2. [Related works](#related-works)
3. [Methodology](#methodology)
4. [Experiment Setup and Implementation](#experiment-setup-and-implementation)
5. [Results and Analysis](#results-and-analysis)
6. [Conclusion](#conclusion)
7. [Publications](#publications)
8. [Links](#links)

---

<!-- 
DELETE THIS SAMPLE before publishing to GitHub Pages !!!
This is a sample image, to show how to add images to your page. To learn more options, please refer [this](https://projects.ce.pdn.ac.lk/docs/faq/how-to-add-an-image/)
![Sample Image](./images/sample.png) 
-->


## Abstract
Voluntary computing has emerged as a transformative paradigm, leveraging idle computational resources from personal devices to tackle complex problems. This research proposal advocates for kernel-level architectural innovations to address the limitations of current WebRTC-based personal voluntary computing systems. By integrating virtualization, intelligent task scheduling, and fault tolerance mechanisms directly into the Linux kernel, we aim to unlock unprecedented performance, security, and reliability for ad-hoc distributed computing networks. Our approach seeks to democratize access to exascale computational capabilities while ensuring robust resource isolation and efficient process migration across heterogeneous devices.
## Related works
Several projects have explored voluntary computing at the user level, such as BOINC and Pando. BOINC focuses on large-scale scientific volunteer computing using centralized scheduling, while Pando enables ad-hoc task distribution via browser-based WebRTC. However, these systems face limitations in terms of performance overhead, scalability, and user control due to their reliance on user-space tools and protocols. Our approach builds on the motivation of these systems but distinguishes itself by embedding voluntary computing capabilities directly into the OS kernel for improved responsiveness, system-level coordination, and resource isolation.
## Methodology
Our system enables dynamic role assignment, where any device can become a controller (task dispatcher) or worker (resource contributor) based on its current workload. The core components include:

Role Manager – Determines the device's role at runtime.

Task Scheduler – Splits and assigns workloads to idle devices securely.

Resource Monitor – Tracks CPU, memory, and I/O usage to avoid interference with local tasks.

Worker Daemon – Executes remote tasks in a sandboxed environment and reports results.

Communication Module – Uses lightweight, secure transport protocols for peer discovery and data exchange.

Security Layer – Enforces access control, authentication, and encryption at the kernel level.

Kernel modules interact with user-space utilities to provide a seamless interface for task submission, control, and monitoring. Container-based virtualization (LXC or namespaces with cgroups) is used to isolate worker tasks.
## Experiment Setup and Implementation
To evaluate the effectiveness of the proposed kernel-level voluntary computing system, we plan the following setup:

Base OS: We will select and modify a lightweight Linux distribution (e.g., Alpine or Tiny Core) to incorporate the proposed voluntary computing modules.

Network Environment: A local area network (LAN) will be established using 4–6 heterogeneous devices (e.g., laptops, Raspberry Pis) connected via Wi-Fi to simulate real-world ad-hoc environments.

Task Benchmark: We intend to use simple yet representative distributed computing tasks (such as matrix multiplication and hash computation) to assess the system’s task distribution, latency, and fault recovery mechanisms.

Measurement Tools: Custom monitoring utilities will be developed to log key performance metrics such as CPU usage, task completion time, bandwidth consumption, and task migration behavior.

Evaluation Criteria: The system will be evaluated based on its resource utilization efficiency, ability to handle node churn and failures, scalability under increasing workloads, and impact on the host system’s responsiveness.
## Results and Analysis

## Conclusion

## Publications
[//]: # "Note: Uncomment each once you uploaded the files to the repository"

<!-- 1. [Semester 7 report](./) -->
<!-- 2. [Semester 7 slides](./) -->
<!-- 3. [Semester 8 report](./) -->
<!-- 4. [Semester 8 slides](./) -->
<!-- 5. Author 1, Author 2 and Author 3 "Research paper title" (2021). [PDF](./). -->


## Links

[//]: # ( NOTE: EDIT THIS LINKS WITH YOUR REPO DETAILS )

- [Project Repository](https://github.com/cepdnaclk/e19-4yp-Voluntary-Computing-Operating-System)
- [Project Page](https://cepdnaclk.github.io/e19-4yp-Voluntary-Computing-Operating-System)
- [Department of Computer Engineering](http://www.ce.pdn.ac.lk/)
- [University of Peradeniya](https://eng.pdn.ac.lk/)

[//]: # "Please refer this to learn more about Markdown syntax"
[//]: # "https://github.com/adam-p/markdown-here/wiki/Markdown-Cheatsheet"
