
# Multi-Container Runtime with Kernel Memory Monitor

## 1. Team Information
- Name 1: Saanvi Samayam
- Name 2: Rishab Naveen
- Course: Operating Systems Project

---

## 2. Project Summary

This project implements a lightweight Linux container runtime in C along with a kernel-space memory monitor.

The system supports:
- Running multiple containers simultaneously
- Supervisor-based container management
- Logging using pipes and threads
- Kernel-level memory monitoring with soft and hard limits

The project consists of two main components:

### User-Space Runtime (engine.c)
- Creates containers using clone()
- Uses chroot for filesystem isolation
- Provides CLI commands (start, ps, logs)
- Maintains container metadata
- Communicates with kernel module using ioctl

### Kernel-Space Monitor (monitor.c)
- Linux Kernel Module (LKM)
- Tracks container processes using PID
- Periodically checks memory usage (RSS)
- Enforces soft and hard memory limits

---

## 3. Build, Load, and Run Instructions

### Install Dependencies
```bash
sudo apt update
sudo apt install -y build-essential linux-headers-$(uname -r)
````

### Build Project

```bash
make
```

### Load Kernel Module

```bash
sudo insmod monitor.ko
```

### Verify Device

```bash
ls -l /dev/container_monitor
```

### Setup Root Filesystem

```bash
mkdir rootfs-base
wget https://dl-cdn.alpinelinux.org/alpine/v3.20/releases/x86_64/alpine-minirootfs-3.20.3-x86_64.tar.gz
tar -xzf alpine-minirootfs-3.20.3-x86_64.tar.gz -C rootfs-base

cp -a rootfs-base rootfs-alpha
cp -a rootfs-base rootfs-beta
```

### Start Supervisor

```bash
sudo ./engine supervisor ./rootfs-base
```

### Start Containers

```bash
sudo ./engine start alpha rootfs-alpha /bin/sh
sudo ./engine start beta rootfs-beta /bin/sh
```

### View Containers

```bash
sudo ./engine ps
```

### View Logs

```bash
cat logs/alpha.log
```

### Run Memory Test

```bash
cp memory_hog rootfs-alpha/

sudo ./engine start alpha rootfs-alpha /memory_hog --soft-mib 20 --hard-mib 40
```

### Stop Containers

```bash
sudo ./engine stop alpha
```

### Unload Module

```bash
sudo rmmod monitor
```

---

## 4. Demo with Screenshots

### 1. Multi-container supervision
<img width="727" height="80" alt="image" src="https://github.com/user-attachments/assets/69723299-b6c2-48b1-9c3b-dae6d03e919e" />

Multiple containers (alpha, beta) running under a single supervisor.

### 2. Metadata tracking
<img width="727" height="135" alt="image" src="https://github.com/user-attachments/assets/222d2676-4bef-4373-ba4e-694e195267a4" />

`engine ps` shows container ID, PID, and status.

### 3. Logging
<img width="727" height="44" alt="image" src="https://github.com/user-attachments/assets/64c890ed-0cb8-4f8d-ba7e-5fe5f6220d14" />

Logs captured via pipe-based logging system and stored in log files.

### 4. CLI and IPC
<img width="714" height="137" alt="image" src="https://github.com/user-attachments/assets/54b1b863-793b-4521-94f3-ec8b510e3660" />

CLI commands interact with supervisor to start and manage containers.

### 5 & 6. Memory Monitoring (Soft and Hard Limits)
<img width="925" height="183" alt="image" src="https://github.com/user-attachments/assets/44342657-007c-495a-abd9-6b2929d6fe36" />

* Soft Limit: Kernel logs a warning when memory exceeds soft limit.
* Hard Limit: Kernel kills process when memory exceeds hard limit.

Example output:

```
[monitor] Registered PID XXXX
[monitor] PID XXXX exceeded soft limit
[monitor] PID XXXX killed (hard limit)
```

### 7. Scheduling Experiment
<img width="709" height="205" alt="image" src="https://github.com/user-attachments/assets/54a5447d-b272-4240-8032-7e940200701b" />
<img width="709" height="204" alt="image" src="https://github.com/user-attachments/assets/494e0f70-6958-4536-9ed5-817666abac3e" />

CPU-bound (`cpu_hog`) and I/O-bound (`io_pulse`) workloads were run simultaneously. Differences in execution behavior demonstrate Linux scheduler handling.

### 8. Clean Teardown
<img width="737" height="112" alt="image" src="https://github.com/user-attachments/assets/49d9f9c9-897f-4194-95c3-3362609f078d" />

All processes are properly reaped. No zombie processes observed using:

```
ps aux
```

---

## 5. Engineering Analysis

### Isolation Mechanisms

Isolation is achieved using:

* Namespaces (UTS, Mount)
* chroot for filesystem isolation

All containers share the same kernel but have isolated views of the filesystem and hostname.

---

### Supervisor and Process Lifecycle

The supervisor manages container lifecycle:

* Creates containers using clone()
* Tracks metadata
* Reaps child processes
* Handles signals for clean termination

---

### IPC, Threads, and Synchronization

Two IPC mechanisms are used:

* Pipes for logging
* CLI interaction via process execution

Producer-consumer model:

* Producer reads container output
* Consumer writes to log files

Mutexes ensure safe access to shared buffers.

---

### Memory Management and Enforcement

RSS (Resident Set Size) measures physical memory used by a process.

Soft limit:

* Warning only

Hard limit:

* Process terminated using SIGKILL

Kernel-space enforcement ensures accurate and reliable monitoring.

---

### Scheduling Behavior

Different workloads show:

* CPU-bound tasks consume more CPU time
* I/O-bound tasks yield CPU frequently

Linux scheduler balances fairness and responsiveness.

---

## 6. Design Decisions and Tradeoffs

### Namespace Isolation

* Used chroot instead of pivot_root
* Tradeoff: simpler but less secure

### Supervisor Design

* Simple loop-based supervisor
* Tradeoff: no persistent IPC, but easier implementation

### Logging

* Pipe-based logging with threads
* Tradeoff: complexity vs reliability

### Kernel Monitor

* Timer-based monitoring
* Tradeoff: slight delay vs simplicity

### Scheduling

* Used nice values
* Tradeoff: coarse control vs simplicity

---

## 7. Scheduler Experiment Results

Two workloads tested:

| Workload | Behavior           |
| -------- | ------------------ |
| cpu_hog  | High CPU usage     |
| io_pulse | Periodic I/O waits |

Observation:

* CPU-bound process dominates CPU
* I/O-bound process remains responsive

---

## Final Notes

This project demonstrates integration of:

* User-space process management
* Kernel-space monitoring
* IPC and synchronization
* Linux system behavior

```
