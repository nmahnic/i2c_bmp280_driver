echo "******************************************************"
echo "                      COMPILAR                        "
echo "******************************************************"
echo "dtc -I dts -O dtb -o am335x-boneblack-td3.dtb am335x-td3.dts"
dtc -I dts -O dtb -o am335x-boneblack-td3.dtb am335x-td3.dts
echo "dtc -I dts -O dtb -o am335x-boneblack.dtb am335x.dts"
dtc -I dts -O dtb -o am335x-boneblack.dtb am335x.dts
echo "******************************************************"
echo "          COPIA A /boot/dtbs/4.19.94-ti-r36/          "
echo "******************************************************"
echo "sudo cp am335x-boneblack-td3.dtb /boot/dtbs/4.19.94-ti-r36/"
sudo cp am335x-boneblack-td3.dtb /boot/dtbs/4.19.94-ti-r36/
echo "sudo cp am335x-boneblack.dtb /boot/dtbs/4.19.94-ti-r36/"
sudo cp am335x-boneblack.dtb /boot/dtbs/4.19.94-ti-r36/