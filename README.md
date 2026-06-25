# Hardware 
KD101WU88FP\
How to use cm4 driver a 10.1 inch 1200*1920 display and touch panel（HX8279+GT928)\
We provide industrial tft lcd and touch panel, and made a convert board to connect our panel to CM4/CM5 directly.\
<img width="425" height="58" alt="image" src="https://github.com/user-attachments/assets/677e2c1c-a3a4-4136-8d96-9307796879a9" />\
(https://www.kadidisplay.com/products/10-1-inch-12001920-dsi-mipi-display-for-raspberry-pi%ef%bc%88cm4-cm5%ef%bc%89/)
# Software
# Step 1：
· Use the official Raspberry Pi image burning tool, Raspberry Pi Imager, to download the latest version image to the SD card.\
· And download VMware virtual machine and configure the Ubuntu LTS version image as a cross-compiler.\
<img width="125" height="151" alt="image" src="https://github.com/user-attachments/assets/033d8e9b-b6dc-4235-aa8d-42f7873e998a" />\
# Step 2：
· Pull the latest Raspberry Pi source code in a virtual machine. \
<img width="627" height="79" alt="image" src="https://github.com/user-attachments/assets/8dab5eae-6d12-4cfa-a586-497193d5bb1f" /> \
· And git clone our driver and device tree and place them in the corresponding directory \
<img width="830" height="63" alt="image" src="https://github.com/user-attachments/assets/2e8c675f-1dac-467d-b9c0-e045e62a48eb" />
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
<img width="430" height="37" alt="image" src="https://github.com/user-attachments/assets/b3614f7d-3b32-4d8c-ab12-3a92414466ce" />

# Note:
Kadi Display is a solutions-oriented display provider. We offer in-house production and design of LCD and touch panels, along with Raspberry Pi-ready display drivers, enabling developers and customers to quickly deploy and implement their applications.
# Contact：sales@sz-kadi.com|Whatsapp：+86-13662585086
