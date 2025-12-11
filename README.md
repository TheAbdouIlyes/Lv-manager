# Intelligent LVM Management System

This project demonstrates **automatic and intelligent management of LVM partitions** on a Lubuntu virtual machine.  
It includes **monitoring, cleaning, extending, and balancing LVs** to prevent storage issues.

---

## **Project Objective**

The goal of this project is to manage **LVM logical volumes (LVs)** efficiently by:

1. Monitoring LV usage.
2. Automatically cleaning unnecessary files.
3. Extending LVs from available VG space.
4. Moving files to other LVs if needed.
5. Alerting the administrator when manual intervention is required.

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
2. Volume Group (VG)

The PVs are combined into a Volume Group named ilyes_vg:
```bash
sudo vgcreate ilyes_vg /dev/sdb /dev/sdc
```

VG total size = 40GB.

3. Logical Volumes (LVs)

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
4. Filesystem & Mount

Each LV is formatted with ext4 and mounted:
```bash
sudo mkfs.ext4 /dev/ilyes_vg/data1
sudo mkdir -p /mnt/data1
sudo mount /dev/ilyes_vg/data1 /mnt/data1
```

Entries are added to /etc/fstab to make mounts persistent.

5. Intelligent Scripts

Three main scripts are used:

1. lvm_fill.sh

Simulates filling an LV with data for testing:
```bash
sudo /usr/local/bin/lvm_fill.sh /mnt/data1 12G
```
2. lvm_manager.sh

Performs management actions on an LV:

Cleanup unnecessary files

Extend LV from VG free space

Move files to other LVs with space

Alert admin if nothing works

3. lvm_monitor.sh

Monitors LV usage periodically:

Checks if LV usage ≥ 80%

Calls lvm_manager.sh for intelligent action

Logs actions to /var/log/lvm_monitor.log

6. Cron Job

Monitoring is automated with cron, running every 2 minutes:
```bash
*/2 * * * * /usr/local/bin/lvm_monitor.sh 80 >> /var/log/lvm_monitor.log 2>&1
```
Intelligent Decision Flow

The system follows this priority order:

Check if LV usage ≥ threshold (80%)

Cleanup unnecessary files

Extend LV using free space from the VG

Move files to other LVs with available space

Alert admin if no action can free space

Testing the System

Fill an LV:
```bash
sudo /usr/local/bin/lvm_fill.sh /mnt/data1 12G
```

Wait for cron or manually run monitor:
```bash
sudo /usr/local/bin/lvm_monitor.sh 80
```

Check /var/log/lvm_monitor.log for actions:

Cleanup performed

LV extended

Files moved

Alerts generated if necessary
