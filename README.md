# EGH456-Assignment-2
EGH456 Group Assignment

## Project Description

This project involves designing and implementing an embedded control system for an electric vehicle motor platform using the Tiva C Launchpad and supporting hardware. The system includes low-level BLDC motor commutation, high-level motor state and speed control, user interaction through an LCD touchscreen, and sensor integration through I2C peripherals. The project is developed as a modular team-based system so that each subsystem can be implemented and tested as independently as possible before final integration.

## Team Work Split

- **Person 1:** Motor hardware, hall sensors, and BLDC commutation
- **Person 2:** Motor control logic, FSM, speed control, and fault handling
- **Person 3:** LCD display and touchscreen user interface
- **Person 4:** I2C sensors and peripheral drivers

To reduce dependency on the single physical motor, the project is split so that only one person needs regular access to the motor hardware. The other team members can work mostly independently using agreed interfaces, mock data, and their own peripherals.

### Person 1 — Motor Hardware + Commutation Lead
**Main responsibility:** Low-level motor control on real hardware.

**Tasks**
- Set up hall sensor inputs
- Configure hall sensor interrupts
- Decode hall sensor states
- Map hall states to the correct motor phase outputs
- Call `updateMotor()` on hall transitions
- Verify the motor spins correctly on hardware
- Capture real hall/speed test data for the team

**Main files/modules**
- `motor_commutation.c`
- `motor_commutation.h`
- hall ISR code
- low-level motor hardware code

**Hardware dependency**
- Requires the physical motor regularly

**Deliverables**
- Working hall sensor handling
- Working commutation logic
- Verified motor rotation on hardware
- Real motor feedback data for integration/testing

---

### Person 2 — Control Logic Lead
**Main responsibility:** High-level motor behaviour and control logic.

**Tasks**
- Implement the motor state machine
  - Idle
  - Starting
  - Running
  - E-Stop Braking
  - Fault Latched
- Implement start/stop logic
- Implement E-stop and fault acknowledgement behaviour
- Implement target speed ramping
- Implement normal deceleration and E-stop deceleration
- Use feedback values to control transitions where needed

**Main files/modules**
- `motor_control.c`
- `motor_control.h`
- shared enums/interfaces in `system_interfaces.h`

**Hardware dependency**
- Does not require the physical motor for most development
- Can use mocked speed/state/hall inputs early

**Deliverables**
- Working FSM
- Working speed ramp logic
- Fault handling and braking behaviour
- Clean control APIs for the rest of the team

---

### Person 3 — LCD + Touchscreen UI Lead
**Main responsibility:** User interface and screen interaction.

**Tasks**
- Set up LCD display output
- Set up touchscreen input
- Design screen layout
- Show motor state, speed, and fault status
- Implement button/menu interaction if needed
- Connect user inputs to control requests

**Main files/modules**
- `ui_display.c`
- `ui_display.h`
- `touch_input.c`
- `touch_input.h`

**Hardware dependency**
- Does not require the motor
- Can use fake values for state, speed, and sensor data during development

**Deliverables**
- Working display screens
- Working touch interaction
- Live system status display
- UI integration with control layer

---

### Person 4 — I2C Sensors + Peripheral Lead
**Main responsibility:** Sensor drivers and peripheral support.

**Tasks**
- Set up I2C communication
- Interface with the SHT31 temperature/humidity sensor
- Interface with the VL53L0X distance sensor
- Validate and format sensor outputs
- Provide clean APIs for other modules
- Assist with general peripheral-level setup if needed

**Main files/modules**
- `i2c_sensors.c`
- `i2c_sensors.h`
- helper driver/peripheral files

**Hardware dependency**
- Does not require the motor
- Requires sensor hardware only

**Deliverables**
- Working sensor drivers
- Valid temperature and distance readings
- Reusable sensor APIs
- Sensor data ready for UI or control integration

---
## File Structure Overview

The project is organised by subsystem so each team member can work as independently as possible before final integration.

- **`include/`**  
  Shared header files, public APIs, common enums/structs, and project-wide configuration.

- **`src/control/`**  
  High-level motor control logic, including the state machine, speed control, braking, and fault handling.

- **`src/motor/`**  
  Low-level motor hardware code, including hall sensor handling, commutation, and motor feedback.

- **`src/ui/`**  
  LCD display and touchscreen interface code.

- **`src/sensors/`**  
  I2C sensor drivers and sensor data handling.

- **`src/drivers/`**  
  Low-level hardware drivers, board-support code, and third-party driver code.

- **`src/rtos/`**  
  FreeRTOS-related code such as task creation, queues, and synchronisation setup.

- **`src/utils/`**  
  Small shared helper functions such as conversions, logging, and debounce utilities.

- **`src/main.c`**  
  Main program entry point and top-level integration.

- **`src/startup_gcc.c`**  
  Startup and interrupt vector setup for the target platform.

- **`platformio.ini`**  
  PlatformIO project configuration.

- **`README.md`**  
  Project overview, team split, hardware requirements, and setup notes.
---

## Shared Files

| File | Purpose | Owner |
|---|---|---|
| `system_interfaces.h` | Shared enums, structs, and function prototypes | Agreed by whole team |
| `main.c` | Final integration point for all modules | Shared |
| `motor_commutation.c/.h` | Low-level motor and hall logic | Person 1 |
| `motor_control.c/.h` | FSM, speed control, and fault logic | Person 2 |
| `ui_display.c/.h` | LCD display logic | Person 3 |
| `touch_input.c/.h` | Touchscreen handling | Person 3 |
| `i2c_sensors.c/.h` | I2C sensor drivers | Person 4 |

---

## Dependencies

| Module | Can be developed with mocks? | Notes |
|---|---|---|
| Motor commutation | Partly | Real validation requires the motor |
| FSM + speed control | Yes | Can use fake speed/hall/state inputs |
| LCD + touchscreen UI | Yes | Can use fake motor/sensor values |
| I2C sensors | Mostly | Requires sensors, but not the motor |

---
## Suggested APIs by Module
### 1. Motor Hardware / Commutation API

Owned by Person 1

These functions expose real motor feedback and low-level motor actions.
```
void motor_hw_init(void);
void motor_hw_update_commutation(uint8_t hall_state);
uint8_t motor_hw_get_hall_state(void);
float motor_hw_get_speed_rpm(void);
bool motor_hw_is_running(void);
void motor_hw_stop(void);
```
### 2. Control Logic API

Owned by Person 2

These functions handle high-level motor behaviour and control requests.
```
void motor_control_init(void);
void motor_control_step(void);

void motor_control_request_start(void);
void motor_control_request_stop(void);
void motor_control_trigger_estop(void);
void motor_control_ack_fault(void);

motor_state_t motor_control_get_state(void);
float motor_control_get_target_speed(void);
```
### 3. UI API

Owned by Person 3

These functions manage display updates and user input handling.
```
void ui_init(void);
void ui_update(void);
void ui_show_state(motor_state_t state);
void ui_show_speed(float target_speed_rpm, float actual_speed_rpm);
void ui_show_sensor_data(float temperature_c, float distance_mm);
```
### 4. Sensor API

Owned by Person 4

These functions provide sensor readings to the rest of the system.
```
void sensors_init(void);
bool sensors_update(void);
float sensor_get_temperature(void);
float sensor_get_distance_mm(void);
bool sensor_data_valid(void);
```
---

## Integration Order

To avoid bottlenecks, modules should be integrated in the following order:

1. Motor hardware and hall/commutation layer
2. FSM and speed control using mock inputs
3. I2C sensor drivers
4. LCD/touchscreen UI using fake values
5. Final integration of:
   - real motor feedback into control
   - control outputs into UI
   - sensor values into UI and/or control logic

---

## Ownership Rules

To avoid overlap and merge conflicts:

- **Person 1 owns** hall reading, motor phase switching, and low-level motor hardware functions
- **Person 2 owns** motor state, control decisions, braking behaviour, and target speed logic
- **Person 3 owns** UI layout, touchscreen interaction, and display logic
- **Person 4 owns** raw sensor communication and sensor read functions

### Important boundaries
- Person 3 should not directly implement sensor I2C drivers
- Person 4 should not decide UI layout
- Person 2 should not directly edit motor commutation logic
- Person 1 should not own the high-level FSM

---

## Team Rules

- Agree on function names and shared interfaces before coding
- Avoid uncontrolled shared global variables
- Each module should expose clean getter/setter functions where needed
- Test each module independently before integration
- Only Person 1 should need regular access to the physical motor
- Nominate one merge/integration lead for each milestone

---
## 🔧 Git Workflow Guide (Team Usage)

To keep our project clean and avoid merge conflicts, please follow this Git workflow.

### ✅ Basic Rules
- Always pull before starting work
- Work on your own branch (never directly on main)
- Make small, frequent commits
- Open small pull requests (PRs) and merge quickly
- Communicate if working on shared files
### 🌿 Branch Workflow
- Start from the latest main:
```bash
git checkout main
git pull origin main
```
- Create a new branch:
```bash
git checkout -b feature/your-feature-name
```
Work and commit:
```bash
git add .
git commit -m "Describe your change"
```
Push your branch:
```bash
git push origin feature/your-feature-name
```
Open a Pull Request on GitHub
🔄 Keep Your Branch Updated

Before pushing or opening a PR, update your branch:

```bash
git checkout main
git pull origin main
git checkout feature/your-feature-name
git rebase main
```
🚨 If Push Gets Rejected

If you see something like:
```bash
! [rejected] (fetch first)
```
Run:
```bash
git pull --rebase
```
Then push again.

⚙️ Recommended Git Settings (run once)
```bash
git config --global pull.rebase true
git config --global rebase.autoStash true
```
These help keep history clean and avoid unnecessary conflicts.

🧠 Team Tips
Don’t work on the same file at the same time without communicating
Avoid large PRs — smaller changes are easier to review and merge
Commit often with clear messages
If unsure, ask before merging
📌 Golden Rule

Always sync with main before pushing your work.
