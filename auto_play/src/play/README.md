# README

## Introduction

this project include two packages: control_tool and rslidar

we add some code in rslidar, so you must build our rslidar package then control tool will be aviliable.

## Dependence

- ros indigo(Ubuntu 14.04)
- qt  5.2.1 or higher

## How to Build


```
% cd ws/src
% git clone git@192.168.1.20:moujiajun/play_tool.git
% cd ..
% catkin_make
    
```

## How to Start

- add a "data" directory in control_tool/ , put your "lidar.pcap" dataset in the "data" directory.

- add a "save_data" dirctory in control_tool/ , the tool can save .pcd data in this directory.

```
% cd control_tool
% mkdir data
% mkdir save_data
% roslaunch control_tool demo.launch
```

##demo

![demo](http://192.168.1.20/moujiajun/program_primer/raw/440d8f5709530ce5d23310438eea6305b90d87fa/img/2017-09-05%2017:47:55%E5%B1%8F%E5%B9%95%E6%88%AA%E5%9B%BE.png) 

