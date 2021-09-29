
# ChangeLog ISVP-T31-1.1.1
----

## [doc]
* Update T31开发指南-20200508.pdf

## [tools]
* Update rmem xls file to v2.2
* Add win10 USB RNDIS driver
* Add jxf37 sc2335 jxq03 bin file
* Update sample sensor bin file

## [u-boot]
* Add nor flash XM25QH128C or GM25Q128A support
* Add gmac phy reset control
* add cmd_watchdog, user can refer to the cmd_watchdog operation watchdog

## [Drivers]
* Update ISP driver
* Update ISP driver wdr or adr parameters
* Fix some bug of isp driver(wdr awb)
* Fix the jxf37 long frame max it bug
* Fix the set fps bug
* Fixed the adr update brighter bug
* Add adr/defog enable  API support
* Add get sensor attr API support
* Add sdns/mdns strength API support
* Support sensor list: c23a98 c4390 gc1034 gc2053 gc2063 gc4653 imx307 imx327 imx335 os02k10 os04b10 os05a10  ov5648 sc2232h sc2235 sc2239 sc2310 sc2315e sc2335 sc3235 sc3335 sc4335 sc5235 jxf23 jxf37 jxh62 jxk03 jxk05 jxq03
* Add hash driver and sample
* Support simultaneous use of analog mic and digital mic, add dmic aec function
* Support both analog mic and digital mic
* Add pga minimum gain configuration in alc mode
* Fix some bug of audio
* Optimization sensor driver Makefile
* Optimization the adr init time
* Fixed the defog black flare bug on high str
* Fixed the it over max it bug when open antiflicker

## [Kernel]
* Add nor flash GM25Q128A or XM25QH128C support
* Delete useless default gpio keys

## [SDK]
* Solve jpeg channel change resolution bus error problem
* Change encoder default rc mode
* Add fuction of showing encoder real-time fps and bitrate in impdbg
* Optimize encoder parameters
* Fix sample bug of encoder
* Add ae it max api
* Add ae luma get API
* Add sdns strength API
* Add sensor attr get API
* Add adr/defog enable API
* Modify the isp apis header file description and remove some unuse apis
* Audio support both analog mic and digital mic
* Repeatable call IMP_AI_DisableNs
* Support dmic channel selection and settings
* Add introduction of ai and ao gain
* Add pga minimum gain configuration in alc mode
* Fix the timeout problem caused by turning off ns when ns is not turned on
* Optimize some functions of SDK
* Add watchdog sample
* Optimization some SDK sample


# ChangeLog ISVP-T31-1.1.0
----

## [doc]
* Update T31开发指南-20200114.pdf
* Add description document of bitstream control

## [tools]
* Update rmem xls file

## [u-boot]
* Support usb burn nand
* Support use pull up/down API

## [Drivers]
* Update ISP driver
* Fix some bug of isp driver(awb_zone)
* Fix the sc3235 image black bug
* Fix the os4b10 bug
* Support sensor list: imx307 imx327 os05a10 sc2232h sc2310 gc2063 gc2053 jxf23 sc2235h sc2235 os04b10 imx355 c4390 sc3235 gc4653 jxf37 jxh62 jxk03 sc2239 sc2315e sc4335 sc5235
* Add sensor hvflip function
* Fixed some isp statistic bug
* Add des driver and sample
* Optimization audio driver
* Optimization sensor driver
* Support dmic channel selection and settings

## [Kernel]
* Add mmc1 configs
* Modify GPIO bound to T31 digital mic
* Support usb burn nand
* Add uart2 configs
* Delete useless configs

## [SDK]
* Add new ratecontrol mode CappedQuality which is similar to CappedVbr but improve the convergence time when scence change
* Improve encode quality when uMaxSameSenceCnt greater than 1;
* Add Encoder scaler and crop attribute in imp_encoder.h
* Add Encoder jpeg share 264/265 stream buffer in imp_encoder.h
* add prequeue mechanism to improve memory usage in 4M
* modify sample_commom.c to add LOW_BITRATE micro to adjust wifi scene
* Support use /tmp/alloc_manager_info /tmp/continuous_mem_info to look rmem use
* Add awb zone statistics get API
* Add af zone weight API
* Add ae_zone_weight and ae_roi_weight API
* Support dmic channel selection and settings
* Add dmic function
* Optimize some functions of SDK
* Optimize encoder parameters
* Fix sample bug of encoder

# ChangeLog ISVP-T31-1.0.2
----

## [u-boot]
* Fix some bugs in the bottom layer
* Support access jffs2 filesystem
* Sfc GPIO driver strength change to 4mA
* Support net phy sz18201

## [Drivers]
* Update ISP driver
* Add AVPU param set
* Support ISP capture RAW picture
* Support sensor list: imx307 imx327 os05a10 sc2232h sc2310 gc2063 gc2053 jxf23 sc2235h sc2235 os04b10 imx355
* Support wdr and linear switching
* Fix bug of dmic init
* Add audio low power configuration
* Fix audio problem of recording data offset


## [Kernel]
* Repair adc bug
* Fix some bugs in the bottom layer
* Support usbcamera

## [SDK]
* Fixed the set module bypass api of ISP
* Add ae comp api of ISP
* Add hv flip api of ISP
* Add mask apis of ISP
* Add IMP_FrameSource_SetFrameDepthCopyType to get nocopyed frame from framesource
* Solve the IMP_FrameSource_GetFrame bug
* Add sample sample-chipid and sample-adc
* Optimize the code of encoder
* Solve the overlap use of frame when nocopy depth frame in framesource
* Resolving respiratory effect
* Optimize encoder code flow control
* Solve the gopLength concurrenet set bug of encoder


# ChangeLog ISVP-T31-1.0.1
----

## [u-boot]
* Fix sfc nor read failed
* Support toolchain 5.4.0

## [Drivers]
* Update ISP driver
* Support sensor list: imx307 imx327 os05a10 sc2232h sc2310 gc2063 gc2053  jxf23
* Support dol and fs wdr


## [Kernel]
* Support SFC NAND and SPI NAND
* Support toolchain 5.4.0

#ChangeLog ISVP-T31-1.0.0
---
## First version
