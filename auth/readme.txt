Auth Test Notes:
----------------
The underlying I2c transport and auth chip reference design were
based on code residing here:

      [root@hepdsw91 DeviceDrivers]# svn info
      Path: .
      Working Copy Root Path: /scratch/fred/repos/hepd/trunk
      URL: https://svn.bose.com/hepd/Shelby/trunk/TI-HSP/DeviceDrivers
      Relative URL: ^/trunk/TI-HSP/DeviceDrivers
      Repository Root: https://svn.bose.com/hepd/Shelby
      Repository UUID: f1958c07-0063-4da9-97d2-35a1706fa849
      Revision: 40758
      Node Kind: directory
      Schedule: normal
      Last Changed Author: ys10812
      Last Changed Rev: 38874
      Last Changed Date: 2017-09-11 15:50:10 -0400 (Mon, 11 Sep 2017)

//!<<<< Referenced:
	https://svn.bose.com/hepd/Shelby/trunk/TI-HSP/DeviceDrivers

//!<<<< took some auth chip register definitions and example code from here...
	TI-HSP/DeviceDrivers/AuthChip/AuthChipDefines.h
	TI-HSP/DeviceDrivers/AuthChip/AuthChipDevice.h
	TI-HSP/DeviceDrivers/AuthChip/AuthChipDevice.cpp
	TI-HSP/DeviceDrivers/I2cDriverTransportChannel.cpp

//!<<<< took this entire file(s)...
	TI-HSP/DeviceDrivers/I2cDevLinuxWrapper.c
	TI-HSP/DeviceDrivers/I2cDevLinuxWrapper.h
