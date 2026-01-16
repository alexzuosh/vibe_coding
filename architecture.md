# LiteGPU Architecture

LiteGPU is a lightweight GPU designed for high performance and efficiency, connected via a **PCIe Gen5** interface.

## Core Components

### 1. Command Processor
- **Command Dispatcher**: Responsible for fetching commands and distributing workloads to the Execution Units.

### 2. Execution Units (EUs)
- **Scalability**: The architecture supports between **8 and 32 Execution Units**.
- **Local Memory**: Each EU is equipped with its own dedicated **L1 Cache**.

### 3. Memory Hierarchy
- **L1 Cache**: Integrated within each Execution Unit.
- **L2 Cache**: A global shared cache accessible by the entire GPU.
- **External Memory**: Supports **GDDR6** memory technology for high-bandwidth access.

### 4. Data Movement
- **DMA Engines**: Features **4 DMA (Direct Memory Access)** engines to handle data transfers between host memory and GPU memory efficiently.

### 5. Microcontroller (MCU)
- **Context Management**: Handles GPU context switching and state management.
- **Task Scheduling**: Orchestrates task execution and scheduling.
- **Power Management**: Manages power states and energy efficiency.

## Topology Diagram

```ascii
      +------------------------------------------+
      |               Host System                |
      +--------------------+---------------------+
                           |
                           v PCIe Gen5
      +--------------------+---------------------+
      |              LiteGPU Core                |
      |                                          |
      |  +-------------+       +--------------+  |
      |  | Command     |       | Micro-       |  |
      |  | Dispatcher  |       | controller   |  |
      |  +------+------+       +-------+------+  |
      |         |                      |         |
      |         v                      v         |
      |  +------------------------------------+  |
      |  |          Interconnect              |  |
      |  +---+-----------+-----------+----+---+  |
      |      |           |           |    |      |
      |  +---+--+    +---+--+     +--+--+ |      |
      |  | DMA  |    | EU 0 | ... | EU N| |      |
      |  | x4   |    | +L1  |     | +L1 | |      |
      |  +------+    +---+--+     +--+--+ |      |
      |                  |           |    |      |
      |              +---+-----------+----+      |
      |              |     L2 Cache       |      |
      |              +---------+----------+      |
      |                        |                 |
      |              +---------+----------+      |
      |              | GDDR6 Interface    |      |
      |              +---------+----------+      |
      |                        |                 |
      +------------------------+-----------------+
                           |
                     +-----+------+
                     | GDDR6 MEM  |
                     +------------+
```

## Future Architecture Improvements (Roadmap)

To improve performance-per-watt and efficiency for the next generation of LiteGPU, the following architectural changes are proposed:

### 1. Tile-Based Rendering (TBR)
- **Concept**: Process rendering in small on-chip tiles (e.g., 16x16 or 32x32 pixels) rather than immediate mode.
- **Benefit**: Keeps data in L1/L2 cache longer, strictly minimizing expensive GDDR6 memory bandwidth usage.

### 2. Hardware-Based Scheduling
- **Concept**: Offload high-frequency scheduling decisions from the Firmware/MCU to a dedicated hardware block.
- **Benefit**: Reduces command submission latency and improves Execution Unit utilization.

### 3. AI Acceleration / Systolic Arrays
- **Concept**: Integrate small 4x4 or 8x8 Systolic Arrays within EUs or as a shared block.
- **Benefit**: efficient execution of matrix operations for AI upscaling (super resolution) and inference tasks without burdening general purpose EUs.

### 4. Memory Compression
- **Concept**: Implement inline lossless compression for color and depth buffers.
- **Benefit**: Effectively increases available memory bandwidth by 20-40%.

### 5. CXL Interface (Upgrade from PCIe)
- **Concept**: Support Compute Express Link (CXL).
- **Benefit**: Enables cache-coherent shared memory between Host CPU and LiteGPU, eliminating the need for explicit DMA copies for many workloads.
