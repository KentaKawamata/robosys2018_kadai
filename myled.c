#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/io.h>

MODULE_AUTHOR("Reinoare");
MODULE_DESCRIPTION("driver for LED control");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");

static dev_t dev;
static struct cdev cdv;
static struct class *cls = NULL;
//bcm2835(cpuの上に他のチップが合わさったcpu) system on chip
//bcm2837 : raspberry pi3 のcpu
//volataile メモリ番地を勝手に変えるなとOSに指定
static volatile u32 *gpio_base = NULL;

//デバイスが挿入された場合に起動する関数
static ssize_t led_write(struct file* filp, const char* buf, size_t count, loff_t* pos){

	char c;
	if(copy_from_user(&c, buf, sizeof(char))){
		return -EFAULT;
	}

	if(c=='0'){ 
		gpio_base[10] = 1 << 25;
	} else if(c == '1'){
		gpio_base[7] = 1 << 25;
	}

	printk(KERN_INFO "led_write is called\n");
	return 1;
}

static ssize_t sushi_read(struct file* filp, char* buf, size_t count, loff_t* pos){

	int size = 0;
	//sushiの絵文字のバイナリ
	//char sushi[] = {0xF0, 0x9F, 0x8D, 0xA3, 0x0A};
	char sushi[] = "sushi tabetai";

	//bufのサイズ bufferのポインタの先頭からsushiを入れる
	if(copy_to_user(buf+size, (const char *)sushi, sizeof(sushi))){
		printk(KERN_INFO "sushi : copy_to_user failed\n");
		return -EFAULT;
	}
	//sushiのバイト数
	size += sizeof(sushi);
	return size;
}

static struct file_operations led_fops = {
	.owner = THIS_MODULE,
	.write = led_write,
	.read = sushi_read
};

//初期化
static int __init init_mod(void){

	int retval;
	
	const u32 led = 25;
	const u32 index = led/10;  //=2
	const u32 shift = (led%10)*3; //15bitに指定 
	const u32 mask = ~(0x7 << shift); //111を15bit分左に寄せる
	
	//予めメモリに割り当ててあるGPIO(ハードウェア入出力)の番地
	//予めハードウェア入出力をメモリに割り当てることでハードウェア処理が楽
	gpio_base = ioremap_nocache(0x3f200000, 0xA0);
        gpio_base[index] = (gpio_base[index] & mask) | 	(0x1 << shift);

	retval = alloc_chrdev_region(&dev, 0, 1, "myled");
	if(retval < 0){
		printk(KERN_ERR "alloc_chrdev_region failed\n");
		return retval;
	}

	printk(KERN_INFO "%s is loaded. major:%d\n", __FILE__,MAJOR(dev));

	cdev_init(&cdv, &led_fops);
	//デバイス関係の関数リストに今回の関数を挿入
	retval = cdev_add(&cdv, dev, 1);
	if(retval < 0){
		printk(KERN_ERR "cdev_add failed. major:%d, minor:%d",MAJOR(dev),MINOR(dev));
		return retval;
	}

	cls = class_create(THIS_MODULE, "myled");
	if(IS_ERR(cls)){
		printk(KERN_ERR "class_create failed. \n");
		return PTR_ERR(cls);
	}
	device_create(cls, NULL, dev, NULL, "myled%d", MINOR(dev));
	return 0;
}

static void __exit cleanup_mod(void){

	cdev_del(&cdv);
	device_destroy(cls, dev);
	class_destroy(cls);
	unregister_chrdev_region(dev, 1);
	printk(KERN_INFO "%s is unloaded. major:%d\n", __FILE__,MAJOR(dev));
}

module_init(init_mod);
module_exit(cleanup_mod);

