#!/bin/bash

lv_path=$1
mount_point=$2
threshold=$3
LOGFILE="/var/log/lvm_monitor.log"

log() {
    echo "$(date) - $1" >> $LOGFILE
}

get_usage() {
    df "$mount_point" | awk 'NR==2 {print $5}' | tr -d '%'
}

# --- 1. Cleanup: delete old temp files ---
log "STEP 1: Cleanup old files in $mount_point"
if [ -d "$mount_point/tmp" ]; then
    deleted=$(find "$mount_point/tmp" -type f -mtime +7 -delete -print | wc -l)
    log "Deleted $deleted old files from $mount_point/tmp"
else
    log "No tmp folder in $mount_point, skipping cleanup"
fi

# --- 2. Check if LV still above threshold ---
used=$(get_usage)
if [ "$used" -lt "$threshold" ]; then
    log "SUCCESS: $mount_point now at $used% (below $threshold%) after cleanup"
    exit 0
fi
log "STEP 2: Still at $used%, continuing..."

# --- 3. Check if VG has free space and extend LV ---
log "STEP 3: Checking VG free space"
vg=$(lvs --noheadings -o vg_name "$lv_path" | tr -d ' ')
vg_free_bytes=$(vgs --noheadings --units g --nosuffix -o vg_free "$vg" | tr -d ' ')

if (( $(awk "BEGIN {print ($vg_free_bytes > 1)}") )); then
    extend=$(awk "BEGIN {print ($vg_free_bytes>2)?2:int($vg_free_bytes)}")
    lvextend -r -L +${extend}G "$lv_path" 2>&1 | tee -a $LOGFILE
    log "SUCCESS: Extended $lv_path by ${extend}G from VG $vg"
    exit 0
fi
log "VG $vg has no free space (${vg_free_bytes}G available)"

# --- 4. Move files to other LVs ---
log "STEP 4: Attempting to move files to other LVs"
for other_mount in /mnt/data1 /mnt/data2 /mnt/data3; do
    if [ "$other_mount" != "$mount_point" ] && mountpoint -q "$other_mount"; then
        free_gb=$(df "$other_mount" | awk 'NR==2 {print $4}')
        free_num=$(echo "$free_gb" | sed 's/[^0-9.]//g')
        
        if (( $(awk "BEGIN {print ($free_num > 5)}") )); then
            # Only move files from a safe subdirectory, not root!
            if [ -d "$mount_point/moveable" ]; then
                moved=$(find "$mount_point/moveable" -maxdepth 1 -type f | head -10)
                if [ -n "$moved" ]; then
                    echo "$moved" | xargs -I {} mv {} "$other_mount"/
                    log "Moved files from $mount_point/moveable to $other_mount"
                    
                    # Check if problem solved
                    used=$(get_usage)
                    if [ "$used" -lt "$threshold" ]; then
                        log "SUCCESS: Now at $used% after moving files"
                        exit 0
                    fi
                fi
            fi
        fi
    fi
done
log "Could not free enough space by moving files"

# --- 5. Check for unused physical disks ---
log "STEP 5: Checking for unused physical disks"
for disk in /dev/sd{d..z}; do
    if [ -b "$disk" ]; then
        # Check if disk is not already a PV
        if ! pvs "$disk" &>/dev/null; then
            log "Found unused disk: $disk"
            
            # Create PV and extend VG
            pvcreate "$disk" 2>&1 | tee -a $LOGFILE
            vgextend "$vg" "$disk" 2>&1 | tee -a $LOGFILE
            
            # Extend LV by 2GB or all available space
            new_free=$(vgs --noheadings --units g --nosuffix -o vg_free "$vg" | tr -d ' ')
            extend=$(awk "BEGIN {print ($new_free>2)?2:int($new_free)}")
            lvextend -r -L +${extend}G "$lv_path" 2>&1 | tee -a $LOGFILE
            
            log "SUCCESS: Added $disk to VG and extended $lv_path by ${extend}G"
            exit 0
        fi
    fi
done
log "No unused physical disks found"

# --- 6. Alert admin ---
log "ALERT: $mount_point is at ${used}% FULL! Manual intervention required!"
echo "$(date) - CRITICAL: $mount_point needs immediate attention" | mail -s "LVM Alert: $mount_point" root
