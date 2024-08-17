#/bin/bash
# This script is used to copy android isp libs and binaries
# from android out folder to vendor/nxp/fsl-proprietary.
# And auto generate a commit which contains current branch and commit info.
# Pre-condition:
# Need source/lunch under android source code root directory firstly.
# Need setup a branch to track remote branch and local branch name must be
# same as remote one.
# This script assmues every commit contains "Signed-off-by:***".

function update_lib()
{
    if [ -e "$SRC_PWD/lib/$1" ];then
        cp $SRC_PWD/lib/$1 $DST_PWD/isp/lib64/$1 && \
        echo "update the file $DST_PWD/isp/lib64/$1 based on $SRC_PWD/lib/$1"
    else
        echo "$SRC_PWD/lib/$1 not exits!"
    fi
}

function update_bin()
{
    if [ -e "$SRC_PWD/bin/$1" ];then
        cp $SRC_PWD/bin/$1   $DST_PWD/isp/bin/$1 && \
        echo "update the file $DST_PWD/isp/bin/$1 based on $SRC_PWD/bin/$1"
    else
        echo "$SRC_PWD/bin/$1 not exits!"
    fi
}

function update_json_os08a20()
{
    CONFIG_DIR=dewarp/dewarp_config
    if [ -e "$CONFIG_DIR/$1" ];then
        mkdir -p $DST_PWD/isp/config_os08a20/dewarp_config && \
        cp $CONFIG_DIR/$1   $DST_PWD/isp/config_os08a20/dewarp_config/$1 && \
        echo "update the file $DST_PWD/isp/config_os08a20/dewarp_config/$1 based on $CONFIG_DIR/$1"
    else
        echo "$CONFIG_DIR/$1 not exits!"
    fi
}

function update_xml_os08a20()
{
    CONFIG_DIR=units/isi/drv/OS08a20/calib/OS08a20
    if [ -e "$CONFIG_DIR/$1" ];then
        cp $CONFIG_DIR/$1   $DST_PWD/isp/config_os08a20/$1 && \
        echo "update the file $DST_PWD/isp/config_os08a20/$1 based on $CONFIG_DIR/$1"
    else
        echo "$CONFIG_DIR/$1 not exits!"
    fi
}

# Once there's anything need to be copied, need add it here.
function update_all()
{
    update_lib liba2dnr.so
    update_lib liba3dnr.so
    update_lib libadpcc.so
    update_lib libadpf.so
    update_lib libaec.so
    update_lib libaee.so
    update_lib libaflt.so
    update_lib libaf.so
    update_lib libahdr.so
    update_lib libappshell_ebase.so
    update_lib libappshell_hal.so
    update_lib libappshell_ibd.so
    update_lib libappshell_oslayer.so
    update_lib libavs.so
    update_lib libawb.so
    update_lib libawdr3.so
    update_lib libbase64.so
    update_lib libbufferpool.so
    update_lib libbufsync_ctrl.so
    update_lib libcam_calibdb.so
    update_lib libcam_device.so
    update_lib libcam_engine.so
    update_lib libcameric_drv.so
    update_lib libcameric_reg_drv.so
    update_lib libcim_ctrl.so
    update_lib libcommon.so
    update_lib libdaA3840_30mc.so
    update_lib libdewarp_hal.so
    update_lib libebase.so
    update_lib libfpga.so
    update_lib libhal.so
    update_lib libi2c_drv.so
    update_lib libibd.so
    update_lib libisi.so
    update_lib libmedia_server.so
    update_lib libmim_ctrl.so
    update_lib libmipi_drv.so
    update_lib libmom_ctrl.so
    update_lib libos08a20.so
    update_lib liboslayer.so
    update_lib libov2775.so
    update_lib libsom_ctrl.so
    update_lib libversion.so
    update_lib libvom_ctrl.so
    update_lib libvvdisplay_shared.so
    update_bin isp_media_server
    update_bin vvext
    update_json_os08a20 sensor_dwe_os08a20_1080P_config.json
    update_json_os08a20 sensor_dwe_os08a20_4K_config.json
    update_xml_os08a20 OS08a20_8M_10_1080p_hdr.xml
    update_xml_os08a20 OS08a20_8M_10_1080p_linear.xml
    update_xml_os08a20 OS08a20_8M_10_4k_hdr.xml
    update_xml_os08a20 OS08a20_8M_10_4k_linear.xml
}


# Auto generate a commit which contains current branch and commit info.
# Commit message format is as below:
# MA-**** ****
# Source git: verisilicon_sw_isp.git
# Source branch: remotes/origin/****
# Source commit: ****
function auto_commit()
{
    COMMIT_ID=`git rev-parse HEAD`
    COMMIT_BRANCH=`git rev-parse --abbrev-ref HEAD`
    TEMP_FILE=`pwd`/temp.txt
    git log -1 --pretty='%B' > ${TEMP_FILE}
    sed -i "s/Change-Id:.*//g" ${TEMP_FILE}
    sed -i "s/Signed-off-by.*/Source git: verisilicon_sw_isp.git\nSource branch: remotes\/origin\/${COMMIT_BRANCH}\nSource commit: ${COMMIT_ID}/g" ${TEMP_FILE}
    sed -i "s/ \[#imx-.*\]//g" ${TEMP_FILE}

    cd $DST_PWD/
    git add *
    git commit -s -F ${TEMP_FILE}
    rm -rf ${TEMP_FILE}
}

if [ -z $OUT ] && [ -z $ANDROID_BUILD_TOP ];then
    echo "Env is not ready! Please go to android source code root directory:"
    echo "source build/envsetup.sh"
    echo "lunch"
    exit
else
    SRC_PWD=appshell/build-android/generated/release
    DST_PWD=$ANDROID_BUILD_TOP/vendor/nxp/fsl-proprietary
    DRIVER_PWD=`pwd`
    update_all
    auto_commit
fi
