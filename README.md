# JIG.m1s.server
odroid-m1s server

### Documentation for JIG development
* Document : https://docs.google.com/spreadsheets/d/14jsR5Y7Cq3gO_OViS7bzTr-GcHdNRS_1258eUWI2aqs/edit#gid=1673179576
* Protocol : https://docs.google.com/spreadsheets/d/1Of7im-2I5m_M-YKswsubrzQAXEGy-japYeH8h_754WA/edit#gid=0

### Base linux image
 * https://dn.odroid.com/RK3566/ODROID-M1S/Ubuntu/ubuntu-20.04-server-odroidm1s-20231030.img.xz

### Server mode settings

* u-boot panel setup (Systen boot and hold '^C' to enter bootloader mode)
```
=> setenv panel display_vu8s
=> saveenv
=> reset
```

* Install ubuntu package & python3 module
```
// ubuntu package
root@server:~# apt install samba ssh build-essential python3 python3-pip ethtool net-tools usbutils git i2c-tools vim cups cups-bsd overlayroot nmap

// python3 package
root@server:~# pip install aiohttp asyncio
```

* Edit the /boot/config.ini
```
root@server:~# vi /boot/config.ini
```
```
[generic]
overlay_resize=16384
overlay_profile=""
# default
# overlays="spi0 i2c0 i2c1"

# JIG Server overlays(Vu8s LCD)
overlays="uart0 i2c0 i2c1 display_vu8s"

# JIG Server overlays(Development for HDMI)
# overlays="uart0 i2c0 i2c1"

# JIG Client overlays
# overlays="fiq0_to_uart2"

[overlay_custom]
overlays="i2c0 i2c1"

[overlay_hktft32]
overlays="hktft32"

[overlay_hktft35]
overlays="hktft35 sx865x-i2c1"
```

* Edit the /etc/default/flash-kernel for the Vu8s rotate
```
root@server:~# vi /etc/default/flash-kernel
```
```
# Development for HDMI
# LINUX_KERNEL_CMDLINE="root=UUID=1f930989-edbf-479f-8eec-8f0be4486610 rootwait ro quiet video=HDMI-A-1:1280x800@60"

# Development for Vu8s
LINUX_KERNEL_CMDLINE="root=UUID=1f930989-edbf-479f-8eec-8f0be4486610 rootwait ro quiet fbcon=rotate:1"
LINUX_KERNEL_CMDLINE_DEFAULTS=""
```
```
// boot script update for new boot.scr
root@server:~# update-bootscript
```

### add root user, ssh root enable (https://www.leafcats.com/176)
```
// root user add
root@server:~# passwd root
```

### server auto login
```
root@server:~# systemctl edit getty@tty1.service
```
```
[Service]
ExecStart=
ExecStart=-/sbin/agetty --noissue --autologin root %I $TERM
Type=idle
```

### server static ip settings
```
root@server:~# vi /etc/netplan/01-netcfg.yaml
```
```
network:
    version: 2
    renderer: networkd
    ethernets:
        eth0:
            dhcp4: no
            # static ip address
            addresses:
                - 192.168.20.162/24
            gateway4: 192.168.20.1
            nameservers:
              addresses: [8.8.8.8,168.126.63.1]

```

### server samba config
```
root@server:~# vi /etc/samba/smb.conf
```
```
[odroidm1s]
   comment = odroid-m1s jig root
   path = /root
   guest ok = no
   browseable = no
   writable = yes
   create mask = 0755
   directory mask = 0755
```

### Sound setup
```
root@server:~# aplay -l
**** List of PLAYBACK Hardware Devices ****
card 0: rockchiphdmi0 [rockchip-hdmi0], device 0: fe400000.i2s-i2s-hifi i2s-hifi-0 [fe400000.i2s-i2s-hifi i2s-hifi-0]
  Subdevices: 1/1
  Subdevice #0: subdevice #0
card 1: rockchiprk809 [rockchip-rk809], device 0: fe410000.i2s-rk817-hifi rk817-hifi-0 [fe410000.i2s-rk817-hifi rk817-hifi-0]
  Subdevices: 1/1
  Subdevice #0: subdevice #0

root@server:~# amixer -c 1
Simple mixer control 'Playback Path',0
  Capabilities: enum
  Items: 'OFF' 'RCV' 'SPK' 'HP' 'HP_NO_MIC' 'BT' 'SPK_HP' 'RING_SPK' 'RING_HP' 'RING_HP_NO_MIC' 'RING_SPK_HP'
  Item0: 'OFF'
Simple mixer control 'Capture MIC Path',0
  Capabilities: enum
  Items: 'MIC OFF' 'Main Mic' 'Hands Free Mic' 'BT Sco Mic'
  Item0: 'MIC OFF'

root@server:~# amixer -c 1 sset 'Playback Path' 'HP'
root@server:~# amixer -c 1
Simple mixer control 'Playback Path',0
  Capabilities: enum
  Items: 'OFF' 'RCV' 'SPK' 'HP' 'HP_NO_MIC' 'BT' 'SPK_HP' 'RING_SPK' 'RING_HP' 'RING_HP_NO_MIC' 'RING_SPK_HP'
  Item0: 'HP'
Simple mixer control 'Capture MIC Path',0
  Capabilities: enum
  Items: 'MIC OFF' 'Main Mic' 'Hands Free Mic' 'BT Sco Mic'
  Item0: 'MIC OFF'

// play audio file
root@server:~# aplay -Dhw:1,0 {audio file} -d {play time}
```

### Clone the reopsitory with submodule
```
root@odroid:~# git clone --recursive https://github.com/charles-park/JIG.m1s.server

or

root@odroid:~# git clone https://github.com/charles-park/JIG.m1s.server
root@odroid:~# cd JIG.m1s.server
root@odroid:~/JIG.m1s.server# git submodule update --init --recursive
```

### Overlay root
* overlayroot enable
```
root@server:~# update-initramfs -c -k $(uname -r)
Using DTB: rockchip/rk3566-odroid-m1s.dtb
Installing rockchip into /boot/dtbs/5.10.0-odroid-arm64/rockchip/
Installing rockchip into /boot/dtbs/5.10.0-odroid-arm64/rockchip/
flash-kernel: installing version 5.10.0-odroid-arm64
Generating boot script u-boot image... done.
Taking backup of boot.scr.
Installing new boot.scr.

root@server:~# mkimage -A arm64 -O linux -T ramdisk -C none -a 0 -e 0 -n uInitrd -d /boot/initrd.img-$(uname -r) /boot/uInitrd 
Image Name:   uInitrd
Created:      Fri Oct 27 04:27:58 2023
Image Type:   AArch64 Linux RAMDisk Image (uncompressed)
Data Size:    7805996 Bytes = 7623.04 KiB = 7.44 MiB
Load Address: 00000000
Entry Point:  00000000

// Change overlayroot value "" to "tmpfs" for overlayroot enable
root@server:~# vi /etc/overlayroot.conf
...
overlayroot_cfgdisk="disabled"
overlayroot="tmpfs"
```
* overlayroot disable
```
// get write permission
root@server:~# overlayroot-chroot 
INFO: Chrooting into [/media/root-ro]
root@server:~# 

// Change overlayroot value "tmpfs" to "" for overlayroot disable
root@server:~# vi /etc/overlayroot.conf
...
overlayroot_cfgdisk="disabled"
overlayroot=""
```
