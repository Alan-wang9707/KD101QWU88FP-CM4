# Hardware 
KD101WU88FP-FC101\
How to use cm4 driver a 10.1 inch 1200*1920 display and touch panel（HX8279+GT928)\
We provide industrial tft lcd and touch panel, and made a convert board to connect our panel to CM4/CM5 directly.\
<img width="666" height="57" alt="image" src="https://github.com/user-attachments/assets/fc5a686e-413c-4123-8c19-01792ad774a7" />\
(https://www.kadidisplay.com/products/10-1-inch-12001920-dsi-mipi-display-for-raspberry-pi%ef%bc%88cm4-cm5%ef%bc%89/)
# Software
# Step 1：
· Use the official Raspberry Pi image burning tool, Raspberry Pi Imager, to download the latest version image to the SD card.\
· And download VMware virtual machine and configure the Ubuntu LTS version image as a cross-compiler.\
<img width="125" height="151" alt="image" src="https://github.com/user-attachments/assets/033d8e9b-b6dc-4235-aa8d-42f7873e998a" />
# Step 2：
· Pull the latest Raspberry Pi source code in a virtual machine. \
<img width="627" height="79" alt="image" src="https://github.com/user-attachments/assets/8dab5eae-6d12-4cfa-a586-497193d5bb1f" /> \
· And git clone our driver and device tree and place them in the corresponding directory \
<img width="740" height="44" alt="image" src="https://github.com/user-attachments/assets/a61dab3f-3559-4411-b56e-59318490d53d" />
# Step 3：
· Compile the kernel and device tree according to the official procedure to generate the corresponding .KO and .dtbo files.\
详情请见：https://www.raspberrypi.com/documentation/computers/linux_kernel.html#kernel
# Step 4：
· Transfer the compiled .ko and .dtbo files to CM4 via SSH or by copying them directly to an SD card.
# Step 5：
· Place the .dtbo file in the /boot/firmware/overlays folder. \
· Place the .ko file in the /lib/modules/$(uname -r)/extra/ folder. \
· sudo depmod -a
#  Step 6：
· Simply modify your config.txt file and add the corresponding dtoverlay.\
<img width="426" height="35" alt="image" src="https://github.com/user-attachments/assets/80cced2c-e80e-4b28-bc36-585923dc2ceb" />

# Note:
Kadi Display is a solutions-oriented display provider. We offer in-house production and design of LCD and touch panels, along with Raspberry Pi-ready display drivers, enabling developers and customers to quickly deploy and implement their applications.
# Contact：sales@sz-kadi.com|Whatsapp：+86-13662585086
