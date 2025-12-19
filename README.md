# Intelligent LVM Management System

This project demonstrates **automatic and intelligent management of LVM partitions** on a Lubuntu virtual machine.  
It includes **monitoring, cleaning, extending, and balancing LVs** to prevent storage issues.

---

## **Project Objective**

The goal of this project is to manage **LVM logical volumes (LVs)** efficiently by:

1. Monitoring LV usage.
2. Automatically cleaning unnecessary files.
3. Extending LVs from available VG space.
4. Moving files to other LVs from the same VG if needed.
5. Alerting the administrator when manual intervention is required.

---

## **Requirements**

Linux (I used lubuntu )

LVM2 (sudo apt install lvm2)

Root privileges or sudo

GCC compiler (sudo apt install build-essential)

---

## **Project Structure**

### 1. Physical Disks (PVs)

Two additional virtual disks are used (20GB each):

- `/dev/sdb`
- `/dev/sdc`

These disks are initialized as **Physical Volumes**:

```bash
sudo pvcreate /dev/sdb /dev/sdc
```
### 2. Volume Group (VG)

The PVs are combined into a Volume Group named ilyes_vg:
```bash
sudo vgcreate ilyes_vg /dev/sdb /dev/sdc
```

VG total size = 40GB.

### 3. Logical Volumes (LVs)

Three LVs are created:

LV Name	Size	Mount Point
data1	15G	/mnt/data1
data2	15G	/mnt/data2
data3	5G	/mnt/data3
```bash
sudo lvcreate -L 15G -n data1 ilyes_vg
sudo lvcreate -L 15G -n data2 ilyes_vg
sudo lvcreate -L 5G -n data3 ilyes_vg
```
### 4. Filesystem & Mount

Each LV is formatted with ext4 and mounted:
```bash
sudo mkfs.ext4 /dev/ilyes_vg/data1
sudo mkdir -p /mnt/data1
sudo mount /dev/ilyes_vg/data1 /mnt/data1

sudo mkfs.ext4 /dev/ilyes_vg/data2
sudo mkdir -p /mnt/data2
sudo mount /dev/ilyes_vg/data2 /mnt/data2

sudo mkfs.ext4 /dev/ilyes_vg/data3
sudo mkdir -p /mnt/data3
sudo mount /dev/ilyes_vg/data3 /mnt/data3

```

Entries are added to /etc/fstab to make mounts persistent.

```
sudo blkid
```
in /etc/fstab we add entries with uuid (we add uuid to ensure that the system always mounts the correct partition even if device names change ex: sdb → sdc)
```
UUID=e4f5b6d7-1234-5678-9abc-def012345678 /mnt/data1 ext4 defaults 0 2
```
same with data2 data3

### 5. C programms :

Three main C programs are used:

1. lvm_fill

Simulates filling an LV with data for testing:
```bash
sudo /usr/local/bin/lvm_fill /mnt/data1 10G
```
2. lvm_manager


Performs management actions on a specific LV:

Cleans temporary files

Extends LV using VG free space

Moves files to other LVs if needed

Alerts admin if no solution is possible


3. lvm_monitor

Monitors LV usage periodically:

Checks if LV usage ≥ 80%

Calls lvm_manager.c for action

Logs actions to /var/log/lvm_monitor.log

### 6. Cron Job

Monitoring is automated with cron, running every 2 minutes:
```bash
*/2 * * * * /usr/local/bin/lvm_monitor 80
```
### Intelligent Decision Flow

The system follows this priority order:

Check if LV usage ≥ threshold (80%)

Cleanup unnecessary files

Extend LV using free space from the VG

Move files to other LVs from same VG with available space

Alert admin if no action can free space

### last step before testing

1- Compile each C program manually

```
gcc -Wall -Wextra -O2 -o lvm_fill lvm_fill.c
gcc -Wall -Wextra -O2 -o lvm_manager lvm_manager.c
gcc -Wall -Wextra -O2 -o lvm_monitor lvm_monitor.c
```

2- Install the programs (move them to /usr/local/bin):

```
sudo cp lvm_fill /usr/local/bin/
sudo cp lvm_manager /usr/local/bin/
sudo cp lvm_monitor /usr/local/bin/
```
3- Create the log file:
```
sudo touch /var/log/lvm_monitor.log
sudo chmod 666 /var/log/lvm_monitor.log

```
### Testing the System



Fill an LV:
```bash
sudo /usr/local/bin/lvm_fill /mnt/data1 14G
```

Wait for cron or manually run monitor:
```bash
sudo /usr/local/bin/lvm_monitor 80
```

Check /var/log/lvm_monitor.log for actions:

Cleanup performed

LV extended

Files moved

Alerts generated if necessary
