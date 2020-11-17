#!/bin/bash

set -x

[[ $1 = zip ]] && { gzip -vc -9 vmlinux > vmlinux.gz; exit 0; }

# checkout rt preempt branch
if [[ $1 = checkout ]]; then
	ts=$(date +%d%b%Y_%Ih%Mm%Ss) #time stamp
	git checkout -b v4.19/standard/preempt-rt/axxia-dev/base${ts} \
		--track origin/v4.19/standard/preempt-rt/axxia-dev/base
	make distclean
	#ls arch/arm64/configs/axxia_xlf_rt_defconfig
	# or 5600. See dir arch/arm64/configs
	make axxia_xlf_rt_defconfig
	exit 0
fi

echo $1
[[ -z $1 ]] && { echo "$0 w024 or $0 frio-037"; exit 0; } || TARGET=$1

#kernel-axx-a078-a15

if [[ $1 =~ ^v ]]; then
	chip=victoria; cpu=a57
elif  [[ $1 =~ ^w ]]; then
	chip=waco; cpu=a53
elif  [[ $1 =~ ^frio ]]; then
	chip=waco_frio
elif [[ $1 =~ ^a ]]; then
	chip=amarillo; cpu=a15
fi


echo $chip $cpu

if [[ $1 =~ ^w || $1 =~ ^v || $1 =~ ^a ]]; then
	kernel=kernel-axx-${TARGET}-${cpu}; echo $kernel
elif [[ $1 =~ ^frio ]]; then
	kernel=kernel-${TARGET}-a15; echo $kernel
fi


if [[ $chip == victoria ]]; then
	ramdisk=false
	case $ramdisk in
		true) multi=multi3_axm56.its; echo 3x;;
		false) multi=multi2_axm56.its; echo 2x;;
	esac
elif [[ $chip == waco ]]; then
	multi=multi2_axc.its; echo $multi
elif [[ $chip == waco_frio ]]; then
	multi=multi2_axc_emu.its
elif [[ $chip == amarillo ]]; then
	multi=multi2_axm55.its
fi

if [ : ]; then
	#make V=1 -j 72 LOCALVERSION=""
	make -j 72 LOCALVERSION=""
else
	make -j 72 || exit
fi
${CROSS_COMPILE}objcopy -O binary -R .note -R .comment -S vmlinux vmlinux.bin
gzip -f -9 vmlinux.bin

set -x
if [[ -e /workspace/sw/mbykowsx/lionfish/iwa_soft_radio-iwa_soft_radio_u-boot/tools/mkimage ]]; then
	/workspace/sw/mbykowsx/lionfish/iwa_soft_radio-iwa_soft_radio_u-boot/tools/mkimage -f $multi multi.fit
else
	/workspace/sw/mbykowsx/lionfish/mkimage -f $multi multi.fit
fi
set +x

: << 'EOF'
ar=( $(grep ^UBOOT_VERSION /workspace/sw/mbykowsx/lionfish/trunk/env/make.env.linux-arma53) )
UBOOT=( $(find /tools/AGRreleases/axxia/ -maxdepth 1 -name "*${ar[2]}*" -type d) )
ar=( $(grep ^KERNEL_VERSION /workspace/sw/mbykowsx/lionfish/trunk/env/make.env.linux-arma53) )
KERNEL=( $(find /tools/AGRreleases/axxia/ -maxdepth 1 -name "*${ar[2]}*" -type d) )
echo ${UBOOT[0]}
echo ${KERNEL[0]}
EOF

#echo "NOT TFTP'ing!!!!!!!"
tftp aus-labsrv2 << TFTP
put multi.fit $kernel
TFTP

set -x

#/workspace/sw/mbykowsx/lionfish/axxia_u-boot_private/tools/mkimage -f multi3.its multi.fit
#cp multi.fit /workspace/sw/mbykowsx/lionfish/uboot


