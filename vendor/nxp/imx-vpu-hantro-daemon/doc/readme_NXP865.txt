1. Environment setup
	
	1.1 system requirement
		On NXP865 board We're running with Linux kernel version 5.4.3.
		There're some incompatible issues with older version of kernel (<4.18).
	
	1.2 kernel configuration
		In building linux kernel there're several configurations that will affect 
	VSI V4L2 system, they are:
		1.2.1 
		VIDEO_V4L2	='y'
		several related definitions: V4L2_FWNODE, V4L2_M2MDEV, VIDEOBUF2_V4L2 should
		be 'm' or 'y'. In NXP's kernel package they're all 'y'.
		1.2.2
		CMA	='y'.
		To enable CMA support in linux.

	1.3 supporting modules
		N.A.
	
	1.4 CMA(contiguous memory area) size
		CMA size could affect memory usage of your V4L2. On NXP865 board default CMA
	is about 1GB. This could support up to 8K single stream encoding.
		
2. Building VSI V4L2 modules
	VSI V4L2 is composed of two modules: kernel driver and daemon. VSI daemon uses
	VC8000E and VC8000D's control software library as supporting library. Pre-built
	libraries are located in /libs4daemon directory.
	
	2.1 VSI V4L2 kernel driver provides standard v4l2 interfaces to user space application.
	It's located in /vsi_v4l2_driver.
		To build VSI V4L2 driver:
			"make clean"
			"make -f Makefie.arm"
		A file named "vsiv4l2.ko" will be created.
		Note that in Makefile.arm "KDIR" should be target board's kernel directory for building.
	
	2.2 Build VC8000E libraries
		2.2.1 build h264 encoder lib:
		vc8000e/system$ make clean libclean h264 DEBUG=n TRACE=n target=TBH V4L2=y CROSS_COMPILE=aarch64-linux-gnu- USE_SAFESTRIG=n
		
		2.2.2 copy the h264 c-model and control SW lib:
		vc8000e$ cp system/models/ench2_asic_model.a $libs4daemon/ench2_asic_model_h264_aarch64.a
		vc8000e$ cp software/linux_reference/libh2enc.a $libs4daemon/libh2enc_aarch64.a
		
		2.2.3 build hevc encoder lib:
		vc8000e/system$ make clean libclean hevc DEBUG=n TRACE=n target=TBH V4L2=y CROSS_COMPILE=aarch64-linux-gnu- USE_SAFESTRIG=n
		
		2.2.4 copy the hevc encoder c-model and control SW lib:
		vc8000e$ cp system/models/ench2_asic_model.a $libs4daemon/ench2_asic_model_hevc_aarch64.a
		vc8000e$ cp software/linux_reference/libh2enc.a $libs4daemon/libh2enc_aarch64.a
	2.3 Build VC8000D libraries
	This script will automatically generate libs, and copy to related path
		2.3.1 build g1 decoder libs:
		./genlib.sh h264high arm_pclinux 
		./genlib.sh h264high versatile 
		./genlib.sh mpeg2 arm_pclinux 
		./genlib.sh mpeg2 versatile 

		2.3.2 build g2 decoder libs:
		./genlib.sh g2dec arm_pclinux 
		./genlib.sh g2dec arm_linux 

	2.4 VSI V4L2 daemon is a daemon process in user space. It's located in /vsi_v4l2_daemon.
		To build VSI V4L2 daemon:
			"make clean"
			"make DECTARGET=xxx ENCTARGET=xxx"
			 DECTARGET is one of nxp_g1dec_hw/nxp_g2dec_hw.
			 ENCTARGET is nxp_h26x_hw.
		A file named "vsidaemon" will be created.
		Please note there're two decoder target formats. The former supports H264, the latter for HEVC.
		All binaries use hardware for codec.
		By default DECTARGET is nxp_g1dec_hw, and ENCTARGET is nxp_h26x_hw.
		
3. Starting up VSI V4L2 system
	3.1 Bring up hardware device file
		mknod /dev/hx280 c 235 0
		mknod /dev/hantrodec c 236 0
	
	3.2 Enable VSI V4L2 driver
		Using these command to enable VSI V4L2 kernel driver in system:
			"insmod vsiv4l2.ko"
			On success it'll create two "/dev/video*" files and "/dev/vsi_daemon_ctrl".
			Formers are	for two V4L2 interface file. On NXP865 board they are
		/dev/video0 and /dev/video1.
			The latter is device handle for communication between VSI V4L2 driver
		and VSI V4L2 daemon.

	3.3 Starting up daemon:
		Just run "vsidaemon"
		On success it won't report any error and blocked wait there.
	
4. Building ffmpeg
		Configure for arm64 platform:
		"./configure --target-os=linux --cross-prefix=aarch64-linux-gnu- --arch=arm64 --pkg-config=pkg-config"
		"make clean all"
		
5. Test examples with ffmpeg
	5.1	Simple encoding:
	$(your_arm64_ffmpeg) -s 352x288 -i foreman_cif.yuv -s 352x288 -vcodec h264_v4l2m2m foreman.h264
	$(your_arm64_ffmpeg) -s 352x288 -i foreman_cif.yuv -s 352x288 -vcodec hevc_v4l2m2m foreman.hevc
	
	5.2	Simple decoding:
	$(your_arm64_ffmpeg) -vcodec h264_v4l2m2m -i foreman.h264 foreman_cif.yuv	(building with DECTARGET=nxp_g1dec_hw)
	$(your_arm64_ffmpeg) -vcodec hevc_v4l2m2m -i foreman.hevc foreman_cif.yuv	(building with DECTARGET=nxp_g2dec_hw)

	5.3 Transcoding:
	$(your_arm64_ffmpeg) -vcodec h264 -i stream.h264 -pix_fmt nv12 -vcodec h264_v4l2m2m -f h264 transcoded.h264
	
	5.4 Multi encoding output:
	$(your_arm64_ffmpeg) -s 352x288 -i foreman_cif.yuv -s 352x288 -vcodec h264_v4l2m2m foreman.h264 -s 176x144 -vcodec h264_v4l2m2m foreman2.h264
	
	5.5 Multi decoding output:
	$(your_arm64_ffmpeg) -vcodec hevc_v4l2m2m -i stream422.h264 -s 1280x720 transcoded.yuv -s 720x480 transcoded2.yuv
		
6. Supported features
	6.1 Supported Codec format as encoder's output and decoder's input
		HEVC and H264.
		
	6.2 Supported Raw format as encoder's input and decoder's output
		YUV420, NV12 and NV21.
		
7. Known issues
	7.1 Memory usage
		Currently all daemon internal memory allocation and mapping goes through /dev/ion on NXP865 board.
		All hardware register mapping goes through corresponding hardware driver.
		Due to /dev/ion's limit, when vsidaemon maps buffers of V4L2 driver, it uses /dev/vsi_daemon_ctrl.
		
	7.2 ffmpeg encoding
		Since ffmpeg does not support B frame in encoding, we haven't tested B-frame encoding with ffmpeg.
		We did test it by our own test code. We'll try to find other public tools which may supports
		B-frame in encoding.
		
	7.3 ffmpeg decoding
		Due to some problem in ffmpeg, there might be a few redundant frames at tail of decoding.
		
	7.4 Decoding output order
		VSI V4L2 decoded stream are in codec order, not display order. Application should be able to handle this.