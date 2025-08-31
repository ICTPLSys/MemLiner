# RemoteSwap
RDMA-based remote swap system for disaggregated cluster

## Updated on May 25

Now the code on the master branch of Remoteswap and the kernel should be good to go.
Make sure to pull the latest commits on the master branch for both repos, on both CPU server and memory server.

### Build & Install

Basically no update to the description below. Build `remoteswap/server` on memory server and `remoteswap/client` on CPU server.
Simply running `make` under both directories should work.

#### One more thing:

RemoteSwap supports debugging using local memory as a fake "remote memory pool". In such case, there is no "remoteswap-server", and the "remoteswap-client" should be built in the following way:

```bash
# (under the remoteswap/client dir)
make BACKEND=DRAM
```

### Usage

The usage is pretty **different** from before, so following the instructions below.
1. On memory server, run `rswap-server` first. This process must be alive all the time so either run it inside `tmux` or `screen`, or run it as a system service.

For now, ou have to know the online core number of the **CPU server** first (sorry this hasn't been automated yet). You can check `/proc/cpuinfo` on the CPU server or simply get the number via `top` or `htop`.
A wrong core number will lead to crash.

```bash
./rswap-server <memory server ip> <memory server port> <memory pool size in GB> <number of cores on CPU server>
# an example: ./rswap-server 10.0.0.4 9400 48 32
```

2. On CPU server, edit the parameters in `manage_rswap_client.sh` under `remoteswap/client` directory.

Here is an excerpt of the script:

```bash
# The swap file/partition size should be equal to the whole size of remote memory
SWAP_PARTITION_SIZE="48"

server_ip="10.0.0.4"
server_port="9400"
swap_file="/mnt/swapfile"
```

Make sure that "SWAP_PARTITION_SIZE" should equal to the remote memory pool size you set when running `rswap-server`, as well as "server_ip" and "server_port" here.

"swap_file" is the path which the script uses to create a special file as a fake swap partition in the size of `SWAP_PARTITION_SIZE`GB. The script will create the file by itself but the path must be valid. The path can be "/mnt/swapfile" here, or somewhere under your home like "${HOME}/aaa/bbb".

3. On CPU server, install the `rswap-client` kernel module, which will establish the connection to the server and finalize the setup.

```bash
./manage_rswap_client.sh install
```

It might take a while to allocate and register all memory in the memory pool, and establish the connection. The system should have been fully set up now.

You can optionally check system log via `dmesg`. A success should look like (1 chunk is 4GB so 12 chunks are essentially 48GB remote memory):
```
rswap_request_for_chunk, Got 12 chunks from memory server.
rdma_session_connect,Exit the main() function with built RDMA conenction rdma_session_context:0xffffffffc08f7460.
frontswap module loaded
```

Setup done!

## configuration

Below are steps to install remoteswap and run Spark on it. We call the server hosting remote memory *remoteswap server* (or *memory server*), and the server running applications with remote memory *remoteswap client* (or *CPU server*).

Please contact Shi Liu (shiliu@g.ucla.edu) for any problems or corrections.

Note: in bash code blocks, **comments starting with "#" are explanations for following commands; comments starting with "##" are explanations of steps which are not simply running a command**

## change kernel

```bash
# on both servers

## set transparent huge pages to madvise

## turn off hyperthreading (logical processor)

# ask Yifan for permission
git clone https://github.com/ivanium/linux-5.4.git
cd linux-5.4
# a good version on branch non-blocking-uffd
git checkout 7e99df
# an alternative newer version on branch opt-uffd-overhead, shouldn't have big difference with first one (not verified)
git checkout 6a6a38

## make sure macro Version#2 is enabled in include/linux/swap_global_macro.h

sudo ./build_kernel.sh build

## change `sudo grub-set-default 0` to `sudo grub2-set-default 0` in build_kernel.sh

sudo ./build_kernel.sh install
sudo reboot now
```

## install MLNX_OFED Driver

```bash
# check CentOS version to see which driver to install
cat /etc/centos-release
# get driver here: https://www.mellanox.com/products/infiniband-drivers/linux/mlnx_ofed
# for CentOS 7.5
wget https://content.mellanox.com/ofed/MLNX_OFED-4.9-2.2.4.0/MLNX_OFED_LINUX-4.9-2.2.4.0-rhel7.5-x86_64.tgz
tar xzf MLNX_OFED_LINUX-4.9-2.2.4.0-rhel7.5-x86_64.tgz
cd MLNX_OFED_LINUX-4.9-2.2.4.0-rhel7.5-x86_64
# for CentOS 7.6
wget https://content.mellanox.com/ofed/MLNX_OFED-4.9-2.2.4.0/MLNX_OFED_LINUX-4.9-2.2.4.0-rhel7.6-x86_64.tgz
tar xzf MLNX_OFED_LINUX-4.9-2.2.4.0-rhel7.6-x86_64.tgz
cd MLNX_OFED_LINUX-4.9-2.2.4.0-rhel7.6-x86_64
# for CentOS 7.7
wget https://content.mellanox.com/ofed/MLNX_OFED-4.9-2.2.4.0/MLNX_OFED_LINUX-4.9-2.2.4.0-rhel7.7-x86_64.tgz
tar xzf MLNX_OFED_LINUX-4.9-2.2.4.0-rhel7.7-x86_64.tgz
cd MLNX_OFED_LINUX-4.9-2.2.4.0-rhel7.7-x86_64
# install dependencies
sudo yum install -y gtk2 atk cairo tcl tk
# install driver. additional packages may need to be installed, just follow the output
sudo ./mlnxofedinstall --add-kernel-support
# update initramfs
sudo dracut -f
# enable driver
sudo /etc/init.d/openibd restart

# to uninstall the driver
sudo ./uninstall.sh
```

## configure IP of InfiniBand

```bash
# on both servers

# configure IP of InfiniBand interface ib0, there are two method to configure the IP
# method 1 is to use Network Manager TUI, see appendix #3
sudo nmtui
# method 2 is to directly modify network scripts, see ref below
# this is the only method if `NM_CONTROLED=no` is set in the script
sudo vim /etc/sysconfig/network-scripts/ifcfg-ib0

# here IP of ib0 on Zion-9 (remoteswap client) is set to 10.0.10.9,
# and  IP of ib0 on Zion-10 (remoteswap server) is set to 10.0.10.10

# check status of infiniband interface
ifconfig ib0
# naive connectivity test, on Zion-9 for example
ping 10.0.10.10
```

ref: [Configuring IP Networking with ifcfg Files](https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux/7/html/networking_guide/sec-configuring_ip_networking_with_ifcg_files)

## install remoteswap module

```bash
# on both servers
git clone https://github.com/ivanium/remoteswap.git
cd ~/remoteswap
# use a verified working commit to proceed
git checkout 773c5b52913f7c13b1c223e6ee2b4c94fe316454

## change IP in code into IP of the memory server
## search "10.0.0.4" in remoteswap folder, replace 2 occurrences of it with "10.0.10.10" (IP of ib0 in Zion-10 here)

# on memory server (remoteswap server), Zion-10 here
cd ~/remoteswap/server
make
./rswap-server

# on CPU server (remoteswap client), Zion-9 here
## change $home_dir in client/manage_rswap_client.sh to your home directory, e.g. `/mnt/ssd/nvme` for me
## replace `$OS_DISTRO` with `"$OS_DISTRO"` in client/manage_rswap_client.sh to avoid errors when $OS_DISTRO contains spaces (e.g. "CentOS Linux")
make
sudo ./manage_rswap_client.sh install
sudo ./manage_rswap_client.sh uninstall
## an reboot is needed for full uninstallation
```

## running Dacapo

### get a copy of Dacapo

1. get in on website: [DaCapo Benchmarks Home Page](http://dacapobench.sourceforge.net)
2. scp it from another server. known copies:
    * shiliu@zion-10.cs.ucla.edu:~/test/dacapo-9.12-MR1-bach.jar (ask Shi for password)
    * shiliu@zion-11.cs.ucla.edu:~/test/dacapo-9.12-MR1-bach.jar (ask Shi for password)

### run dacapo tests

```bash
# print help
java -jar ~/test/dacapo-9.12-MR1-bach.jar -h
# list available benchmark
java -jar ~/test/dacapo-9.12-MR1-bach.jar -l
# list test size of a benchmark
java -jar ~/test/dacapo-9.12-MR1-bach.jar --sizes h2
# run h2 huge sized test
java [java options here ]-jar ~/test/dacapo-9.12-MR1-bach.jar h2 -s huge
```

## running Spark

steps to setup spark environments, here we use Zion-9 as both master and worker. The username below is `nvme`.

```bash
## Get all jars, input data, and Spark. e.g. from shiliu@zion9, ask Shi for password

## JDK12 from Oracle does not include Shenandoah GC support
## so we need to build our own, see appendix #2

# on Zion-9, ask Shi for permission
git clone https://github.com/FereX98/scripts-repo.git`
## copy default config from `scripts-repo/spark-evaluation/{}` to spark config folder, make necessary modifications.
## `slaves`; `spark.master` and `spark.executor.extraJavaOptions` in `spark-defaults.conf`; `JAVA_HOME` and `PATH` in `spark-env.sh`
# use modified `start-slave.sh` to make worker run in cgroup
cp ~/scripts-repo/spark-evaluation/start-slave.sh ~/spark-3.0.0-preview2-bin-hadoop3.2/sbin/start-slave.sh

# create cgroup
sudo cgcreate -t nvme -a nvme -g memory:/memctl
# limit memory used by Spark worker
echo 9g > /sys/fs/cgroup/memory/memctl/memory.limit_in_bytes
# for all local
echo 50g > /sys/fs/cgroup/memory/memctl/memory.limit_in_bytes

# to colocate master and slave on the save machine, need to generate a ssh key pair
cd ~/.ssh
ssh-keygen -f spark
ssh-copy-id -i spark.pub zion-9.cs.ucla.edu
## add lines in appendix #1 in config
code config
chmod 600 config

# start master and worker
~/spark-3.0.0-preview2-bin-hadoop3.2/sbin/start-all.sh
# stop master and worker, restart is required if modifications are made in the config files
~/spark-3.0.0-preview2-bin-hadoop3.2/sbin/stop-all.sh

mkdir ~/logs

# spark apps, jars and dataset used can be found on shiliu@zion9, ask Shi for password
# GraphX PageRank
~/spark-3.0.0-preview2-bin-hadoop3.2/bin/spark-submit --class PageRankExample ~/jars/gpr.jar ~/dataset/out.2g 10 2>&1 | tee -a ~/logs/gp-memliner-test-1.log
# Spark Connected Components
(time -p ~/spark-3.0.0-preview2-bin-hadoop3.2/bin/spark-submit --class ConnectedComponentsExample ~/jars/cc.jar ~/dataset/out.2g 10 2>&1) | tee -a ~/logs/cc-memliner-test-1.log
# Spark PageRank
~/spark-3.0.0-preview2-bin-hadoop3.2/bin/spark-submit --class JavaPageRank ~/jars/pagerank.jar ~/dataset/out.wikipedia_link_pl 10 2>&1 | tee -a ~/logs/spr-memliner-test-2.log
# skewed group by test
~/spark-3.0.0-preview2-bin-hadoop3.2/bin/spark-submit --class org.apache.spark.basic.SkewedGroupByTest ~/jars/sparkapp_2.12-1.0.jar 12 256216 8192 16 2>&1 | tee -a ~/logs/sgbt-memliner-test-2.log
# triangle counting
(time -p ~/spark-3.0.0-preview2-bin-hadoop3.2/bin/spark-submit --class org.apache.spark.basic.SparkTC ~/jars/sparkapp_2.12-1.0.jar 32 3 2>&1) | tee -a ~/logs/tc-shen-9g-1.log
# more apps in auto-run script:
/mnt/ssd/nvme/scripts/spark-evaluation/spark.bash
```

## appendixes

### appendix #1: ssh config

```ssh-config
Host zion-9.cs.ucla.edu
    HostName zion-9.cs.ucla.edu
    User nvme
    IdentityFile ~/.ssh/spark
```

### appendix #2: compile our own JDK

```bash
# install mercurial, version control used by JDK
# on Ubuntu
sudo apt install mercurial
# or on CentOS
sudo yum install mercurial -y

hg clone https://hg.openjdk.java.net/jdk-updates/jdk12u/
cd jdk12u
# list all tags of branch default to choose from
hg log --rev="branch(default) and tag()" --template="{tags}\n"
# jdk-12.0.2+9 is used in MemLiner and ADC
hg update jdk-12.0.2+9
# install dependencies
sudo yum groupinstall -y "Development Tools"
sudo yum install -y libXtst-devel libXt-devel libXrender-devel libXrandr-devel libXi-devel cups-devel fontconfig-devel alsa-lib-devel
sudo yum install -y java-11-openjdk-devel
## select jdk11
sudo alternatives --config java
bash configure --with-debug-level=release --with-target-bits=64
make
# the JDK is located at jdk12u/build/linux-x86_64-server-release/jdk
```

### appendix #3: use network manager TUI to configure IPs

1. input command `sudo nmtui` in a terminal.
2. Select `Edit a connection`
3. Select `InfiniBand` -> `ib0`
4. Input desired IPv4 address in `IPv4 CONFIGURATION` -> `Addresses`, such as `10.0.10.9/24`
5. Set `INFINIBAND` -> `Transport mode` to `Connected`
6. back to main manu and select `Activate a connection`, then `Activate` `ib0`
7. save and exit