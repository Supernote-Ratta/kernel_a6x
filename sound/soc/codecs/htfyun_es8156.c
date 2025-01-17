#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio/consumer.h>

#include "rk817_codec.h"
#include "../rockchip/rockchip_i2s.h"

#include "htfyun_es8156.h"

/**
******* dts config ************
*
    htfyun-es8156@40 {
        compatible = "htfyun-es8156";
        reg = <0x40>;
        enable-gpio = <&gpio0 14  GPIO_ACTIVE_HIGH>;
        status = "okay";
    };

*
********************************
--------- usage:--------
* echo enable_gpio 1 > /sys/class/htfyun-es8156/gpio
* echo 40 4 > /sys/class/htfyun-es8156/reg
*/


static int debug = 1;
module_param(debug, int, S_IRUGO|S_IWUSR);
#define _dprintk(level, fmt, arg...) do {   \
  if (debug >= level)      \
  printk("**htfyun-es8156**%s[%d]: " fmt, __func__, __LINE__, ## arg); } while (0)

#define dprintk(format, ...) _dprintk(1, format, ## __VA_ARGS__)

#define ES8156_NAME "htfyun-es8156"
#define DEFAULT_IS_24BIT_ENABLED (true)

/* private data */
struct es8156_priv {
	struct i2c_client *i2c;
	struct regmap *regmap;

	bool is24bit;

	struct regulator *supply;
	struct gpio_desc *enable_gpio;

	struct mutex mutex_lock_rt5640_mclk;
	bool	rk817_mclk_enabled;

	struct class *cls_node;
	struct class_attribute s_reg;//used for apps, which regmap is not permissive
	struct class_attribute s_gpio;
	struct class_attribute s_is24bit;

	struct delayed_work 	resume;
};

static struct es8156_priv *g_es8156;
static const struct reg_default es8156_reg_defaults[] =  {
	{0x00, 0x1c}, {0x01, 0x20}, {0x02, 0x00}, {0x03, 0x01},
	{0x04, 0x00}, {0x05, 0x04}, {0x06, 0x11}, {0x07, 0x00},
	{0x08, 0x06}, {0x09, 0x00}, {0x0a, 0x50}, {0x0b, 0x50},
	{0x0c, 0x00}, {0x0d, 0x10}, {0x10, 0x40}, {0x10, 0x40},
	{0x11, 0x00}, {0x12, 0x04}, {0x13, 0x11}, {0x14, 0xbf},
	{0x15, 0x00}, {0x16, 0x00}, {0x17, 0xf7}, {0x18, 0x00},
	{0x19, 0x20}, {0x1a, 0x00}, {0x20, 0x16}, {0x21, 0x7f},
	{0x22, 0x00}, {0x23, 0x86}, {0x24, 0x00}, {0x25, 0x07},
	{0xfc, 0x00}, {0xfd, 0x81}, {0xfe, 0x55}, {0xff, 0x10},

};
#if 0
////////////////////////
// 24bit NFS
static const struct reg_sequence es8156_tdm_reg_common_cfg_24bit_nfs[] =  {
	//cfg1
	{ 0x00, 0xFF },
	{ 0x00, 0x32 },
	{ 0x09, 0x30 },
	{ 0x0A, 0x30 },
	{ 0x23, 0x2a },
	{ 0x22, 0x0a },
	{ 0x21, 0x2a },
	{ 0x20, 0x0a },

	{ES8156_MODE_CFG_REG08, 0x10},
	//{ES8156_SDP_CFG1_REG11, 0x01},//16bit
	{ES8156_SDP_CFG1_REG11, 0x81},//32bit
	{ES8156_SDP_CFG2_REG12, 0x07},

	//cfg2
	{ 0x40, 0xC3 },
	{ 0x41, 0x70 },
	{ 0x42, 0x70 },
	{ 0x43, 0x1D },//channel 1 gain 0x19
	{ 0x44, 0x1D },//channel 2 gain 0x19
	{ 0x45, 0x16 },//channel 3 gain
	{ 0x46, 0x16 },//channel 4 gain
	{ 0x47, 0x08 },
	{ 0x48, 0x08 },
	{ 0x49, 0x08 },
	{ 0x4A, 0x08 },
	{ 0x07, 0x20 },

	//{ES8156_MCLK_CTL_REG02, 0xc1},//48K refered to 12.288M
	{ES8156_MCLK_CTL_REG02, 0xc3},//16K refered to 12.288M

	//cfg3
	{ 0x06, 0x04 },
	{ 0x4B, 0x0F },
	{ 0x4C, 0x00 },
	{ 0x00, 0x71 },
	{ 0x00, 0x41 },
	/*
	//mute to see flag
	{ 0x14, 0x03 },
	{ 0x15, 0x03 },
	*/

};

////////////////////////////
// 16bit 1FS
static const struct reg_sequence es8156_tdm_reg_common_cfg_16bit_1fs[] =  {
	//cfg1
	{ 0x00, 0xFF },
	{ 0x00, 0x32 },
	{ 0x09, 0x30 },
	{ 0x0A, 0x30 },
	{ 0x23, 0x2a },
	{ 0x22, 0x0a },
	{ 0x21, 0x2a },
	{ 0x20, 0x0a },

	{ES8156_MODE_CFG_REG08, 0x10},
	{ES8156_SDP_CFG1_REG11, 0x61},//Left Justified
	{ES8156_SDP_CFG2_REG12, 0x02},//TDM I2S/Left Justified

	//cfg2
	{ 0x40, 0xC3 },
	{ 0x41, 0x70 },
	{ 0x42, 0x70 },
	{ 0x43, 0x1E },//channel 1 gain
	{ 0x44, 0x1E },//channel 2 gain
	{ 0x45, 0x16 },//channel 3 gain
	{ 0x46, 0x16 },//channel 4 gain
	{ 0x47, 0x08 },
	{ 0x48, 0x08 },
	{ 0x49, 0x08 },
	{ 0x4A, 0x08 },
	{ 0x07, 0x20 },

	{ES8156_MCLK_CTL_REG02, 0xc1},

	//cfg3
	{ 0x06, 0x04 },
	{ 0x4B, 0x0F },
	{ 0x4C, 0x0F },
	{ 0x00, 0x71 },
	{ 0x00, 0x41 },

};
#endif
static int es8156_read(struct i2c_client *client, u8 reg, u8 *rt_value)
{
	int ret;
	u8 read_cmd[3] = {0};
	u8 cmd_len = 0;

	read_cmd[0] = reg;
	cmd_len = 1;

	if (client->adapter == NULL)
		pr_err("es8156_read client->adapter==NULL\n");

	ret = i2c_master_send(client, read_cmd, cmd_len);
	if (ret != cmd_len) {
		pr_err("es8156_read error1\n");
		return -1;
	}

	ret = i2c_master_recv(client, rt_value, 1);
	if (ret != 1) {
		pr_err("es8156_read error2, ret = %d.\n", ret);
		return -1;
	}

	return 0;
}
static int es8156_write(struct i2c_client *client, u8 reg, unsigned char value)
{
	int ret = 0;
	u8 write_cmd[2] = {0};

	write_cmd[0] = reg;
	write_cmd[1] = value;

	ret = i2c_master_send(client, write_cmd, 2);
	if (ret != 2) {
		pr_err("es8156_write error->[REG-0x%02x,val-0x%02x]\n",reg,value);
		return -1;
	}

	return 0;
}


static int es8156_test_i2c(struct es8156_priv *data)
{
	int reg = ES8156_CHIPID1_REGFD;
	int val = 0;
	int ret = regmap_read(data->regmap, reg, &val);
	dprintk("ES8156_CHIPID1_REGFD = 0x%x", val);
	return ret;
}

/*
 * es8156_reset
 */
static int es8156_reset(struct es8156_priv *data)
{
	es8156_write(data->i2c, ES8156_RESET_REG00, 0x1c);
	msleep(6);
	return es8156_write(data->i2c, ES8156_RESET_REG00, 0x03);
}

int es8156_init_regs(struct es8156_priv *data)
{
	/*reset es8156*/
	#if 0
		es8156_write(data->i2c, ES8156_RESET_REG00,0x3C);
		msleep(10);
		es8156_write(data->i2c, ES8156_RESET_REG00,0x1C);		
	/*
	*set clock and analog power
	*/
		es8156_write(data->i2c, ES8156_SCLK_MODE_REG02,0x04);
		es8156_write(data->i2c, ES8156_ANALOG_SYS1_REG20,0x2A);
		es8156_write(data->i2c, ES8156_ANALOG_SYS2_REG21,0x3C);
		es8156_write(data->i2c, ES8156_ANALOG_SYS3_REG22,0x08);
		es8156_write(data->i2c, ES8156_ANALOG_LP_REG24,0x07);
		es8156_write(data->i2c, ES8156_ANALOG_SYS4_REG23,0x00);
	/*
	*set powerup time
	*/		
		es8156_write(data->i2c, ES8156_TIME_CONTROL1_REG0A,0x01);
		es8156_write(data->i2c, ES8156_TIME_CONTROL2_REG0B,0x01);
	/*
	*set digtal volume
	*/		
		es8156_write(data->i2c, ES8156_VOLUME_CONTROL_REG14,0xBE);
	/*
	*set MCLK
	*/		
		es8156_write(data->i2c, ES8156_MAINCLOCK_CTL_REG01,0x26); //0x26无声？
		es8156_write(data->i2c, ES8156_P2S_CONTROL_REG0D,0x14);
		es8156_write(data->i2c, ES8156_MISC_CONTROL3_REG18,0x00);
		es8156_write(data->i2c, ES8156_CLOCK_ON_OFF_REG08,0x3F);
		es8156_write(data->i2c, ES8156_RESET_REG00,0x02);
		es8156_write(data->i2c, ES8156_RESET_REG00,0x03);
		es8156_write(data->i2c, ES8156_ANALOG_SYS5_REG25,0x20);
		//		es8156_write(data->i2c, ES8156_MAINCLOCK_CTL_REG01,0x26); //0x26无声？
		#else
		es8156_write(data->i2c, 0x02, 0x14); //=
		es8156_write(data->i2c, 0x0A, 0x10); //time
		es8156_write(data->i2c, 0x0B, 0x10); //time
		es8156_write(data->i2c, 0x0D, 0x14); //p2s =
		es8156_write(data->i2c, 0x01, 0x26); //默认MCLK/LRCLK=256，如果MCLK/LRCLK=64，则0x01=0xA0
		es8156_write(data->i2c, 0x08, 0x3F);		
		es8156_write(data->i2c, 0x00, 0x01); 
		es8156_write(data->i2c, 0x20, 0x2A);
		es8156_write(data->i2c, 0x21, 0x3c);
		es8156_write(data->i2c, 0x22, 0x00);	//0x00
		es8156_write(data->i2c, 0x24, 0x07);
		es8156_write(data->i2c, 0x23, 0xFA);  //0xca tanlq 210315 for 1.8V


		es8156_write(data->i2c, 0x0a, 0x01);
		es8156_write(data->i2c, 0x0b, 0x01);
		es8156_write(data->i2c, 0x14, 0xcf);  // 20210617:根据新的硬件调整 817和8156的增益来减低噪音
		es8156_write(data->i2c, 0x18, 0x00);
		es8156_write(data->i2c, 0x08, 0x3f);
		es8156_write(data->i2c, 0x00, 0x02);
		es8156_write(data->i2c, 0x00, 0x03);


		msleep(10);
		es8156_write(data->i2c, 0x25, 0x20); 	//0x20
		es8156_write(data->i2c,ES8156_DAC_SDP_REG11,0x33);
		#endif
		return 0;
}
#if 0
static void es8156_set_rk817_mclk_enabled(struct es8156_priv *data, bool enabled)
{
	bool en;
	mutex_lock(&data->mutex_lock_rt5640_mclk);
	en = !!enabled;
	dprintk("data->rk817_mclk_enabled = %d, en = %d\n", data->rk817_mclk_enabled, en);
	if (data->rk817_mclk_enabled == en) {
		goto _out;
	}
	data->rk817_mclk_enabled = en;
	rk817_set_mclk_enabled(en);
	if (en)
		msleep(100);
_out:
	mutex_unlock(&data->mutex_lock_rt5640_mclk);
}


static int es8156_set_enabled(struct es8156_priv *data, bool enabled)
{
	int ret;
	if (!IS_ERR_OR_NULL(data->supply)) {
		if (enabled) {
			ret = regulator_enable(data->supply);
		} else {
			ret = regulator_disable(data->supply);
		}
		if (ret < 0) {
			dev_err(&data->i2c->dev, "%s:failed to enable supply: %d\n", __func__, ret);
		}
		msleep(10);
	}
	if (!IS_ERR_OR_NULL(data->enable_gpio)) {
		gpiod_set_value(data->enable_gpio, enabled);
		msleep(10);
	}
	return 0;
}

static int es8156_parse_dt(struct es8156_priv *data)
{
	struct device *dev = &data->i2c->dev;

	data->supply = devm_regulator_get(dev, "power");

	if (IS_ERR(data->supply)) {
		dev_warn(dev, "%s: No supply found! continue...\n", __func__);
	}
	data->enable_gpio = devm_gpiod_get_optional(dev, "enable", GPIOD_OUT_LOW);
	if (IS_ERR(data->enable_gpio)) {
		dev_warn(dev, "%s: No enable GPIO found! continue...\n", __func__);
	}
	return 0;
}
#endif
static const struct regmap_config es8156_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.use_single_rw = true,
	.max_register = 0xFF,
	.cache_type = REGCACHE_NONE,
	.reg_defaults = es8156_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(es8156_reg_defaults),
};

static const struct i2c_device_id es8156_i2c_id[] = {
	{ ES8156_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, es8156_i2c_id);

#if defined(CONFIG_OF)
static const struct of_device_id es8156_of_match[] = {
	{ .compatible = ES8156_NAME, },
	{},
};
MODULE_DEVICE_TABLE(of, es8156_of_match);
#endif

//////////////////////////////////////
// called by codec
#define config_reg(data, b) do {\
	regmap_register_patch(data->regmap, es8156_tdm_reg_common_cfg_##b, ARRAY_SIZE(es8156_tdm_reg_common_cfg_##b));\
} while(0)
int es8156_set_dsp_mic_enable(bool enabled)
{
	struct es8156_priv * data = g_es8156;

	if (IS_ERR_OR_NULL(data)) {
		return -ENODEV;
	}
	//es8156_set_rk817_mclk_enabled(data, enabled);
	//msleep(50);

	dprintk("set_dsp_mic_enable enabled = %d. data->is24bit:%d\n", enabled,data->is24bit);
	//es8156_set_enabled(data, enabled);

	if (enabled) {

		//if (data->is24bit) {
		//	es8156_write(data->i2c,ES8156_DAC_SDP_REG11,0x03);
		//} else {
			//es8156_write(data->i2c,ES8156_DAC_SDP_REG11,0x73);
		//}
		//msleep(5);

		//regmap_update_bits(data->regmap, ES8156_ADC34_MUTE_REG14, 0x03, 0x00);
		//regmap_update_bits(data->regmap, ES8156_ADC12_MUTE_REG15, 0x03, 0x00);
	}

	return 0;
}

//////////////////////////////////////
// class node
static ssize_t es8156_class_reg_show(struct class *cls,struct class_attribute *attr, char *_buf)
{
	struct es8156_priv *data = container_of(attr,
					    struct es8156_priv, s_reg);
	ssize_t len = 0;
	char * buf = _buf;
	u8 i;
	int ret;
	u8 val = 0;
	for (i = 0; i <= 0xFF; i++) {
		//ret = regmap_read(data->regmap, i, &val);
		ret = es8156_read(data->i2c, i, &val);
		if (ret < 0) continue;

		len += snprintf(buf + len, PAGE_SIZE - len,
			    "%02x: %02x\n", i, val);
	}
	return len;
}

static ssize_t es8156_class_reg_store(struct class *cls,struct class_attribute *attr, const char *buf, size_t _count)
{
	struct es8156_priv *data = container_of(attr,
					    struct es8156_priv, s_reg);
	int ret;
	int reg;
	int val;
	ret = sscanf(buf, "%x %x", &reg, &val);

	printk("%s:cmd buf:%s\n", __func__, buf );

	if (ret != 2) {
		dev_err(&data->i2c->dev, "%s:param is error, echo reg val > this_node.\n", __func__);
		return _count;
	}
	//regmap_write(data->regmap, reg, val);
	es8156_write(data->i2c, (u8)reg, (unsigned char) val);
	return _count;
}

static ssize_t es8156_class_gpio_show(struct class *cls,struct class_attribute *attr, char *_buf)
{
	struct es8156_priv *data = container_of(attr,
					    struct es8156_priv, s_gpio);
	ssize_t len = 0;
	char * buf = _buf;

	if (IS_ERR_OR_NULL(data->enable_gpio)) {
		len += snprintf(buf + len, PAGE_SIZE - len, "No enable_gpio found.\n");
	} else {
		len += snprintf(buf + len, PAGE_SIZE - len, "enable_gpio = %d\n", gpiod_get_value(data->enable_gpio));
	}

	return len;
}

static ssize_t es8156_class_gpio_store(struct class *cls,struct class_attribute *attr, const char *buf, size_t _count)
{
	struct es8156_priv *data = container_of(attr,
					    struct es8156_priv, s_gpio);
	int ret;
	char name[64];
	int val;

	ret = sscanf(buf, "%s %d", name, &val);
	printk("%s:cmd buf:%s\n", __func__, buf );

	if (ret != 2) {
		dev_err(&data->i2c->dev, "%s:param is error, echo enable_gpio 1 > this_node.\n", __func__);
		return _count;
	}
	if (0 == strcmp(name, "enable_gpio")) {
		if (!IS_ERR_OR_NULL(data->enable_gpio)) {
			gpiod_set_value(data->enable_gpio, !!val);
		}
	}

	return _count;
}

static ssize_t es8156_class_is24bit_show(struct class *cls,struct class_attribute *attr, char *_buf)
{
	struct es8156_priv *data = container_of(attr,
					    struct es8156_priv, s_is24bit);

	ssize_t len = 0;
	char * buf = _buf;

	len += snprintf(buf + len, PAGE_SIZE - len, "%d\n", data->is24bit);

	return len;
}

static ssize_t es8156_class_is24bit_store(struct class *cls,struct class_attribute *attr, const char *buf, size_t _count)
{
	struct es8156_priv *data = container_of(attr,
					    struct es8156_priv, s_is24bit);
	ssize_t ret;
	unsigned int is24bit;

	ret = kstrtouint(buf, 10, &is24bit);
	if (ret)
		goto out_strtoint;

	data->is24bit = !!is24bit;

	es8156_set_dsp_mic_enable(true);

	return _count;
out_strtoint:
	dev_err(&data->i2c->dev, "%s: fail to change str to int\n",
		__func__);
	return ret;
	return _count;
}


#define __STR(x) #x
#define _STR(x) __STR(x)
#define STR(x) _STR(x)

#define CLASS_CREATE_FILE(data,_name_) do{ \
        data->s_##_name_.attr.mode = 0666;\
        data->s_##_name_.attr.name = STR(_name_);\
        data->s_##_name_.show = es8156_class_##_name_##_show;\
        data->s_##_name_.store = es8156_class_##_name_##_store;\
        if (class_create_file(data->cls_node, &data->s_##_name_)) {\
            printk("%s: Fail to creat class file %s\n", __func__, data->s_##_name_.attr.name);\
        }\
    } while(0)

#define CLASS_REMOVE_FILE(data,_name_)       do{ \
        class_remove_file(data->cls_node, &data->s_##_name_);\
    } while(0)
///////////////////////////////////////////////
static void es8156_resume_work(struct work_struct *work)
{
	struct es8156_priv *data = g_es8156;
		//container_of(work, struct es8156_priv, resume.work);
    es8156_reset(data);
	es8156_init_regs(data);		
}


static int es8156_i2c_probe(struct i2c_client *i2c,
		    const struct i2c_device_id *id)
{
	struct es8156_priv *data;
	int ret;
	dev_warn(&i2c->dev, " v20191206-1 enter %s.\n", __func__);
#if 0
	if (get_i2s_state_check() != I2S_STATE_CHECK_3) {
		dev_err(&i2c->dev, "%s, get_i2s_state_check(%d) is not I2S_STATE_CHECK_3(%d), return directly.\n",
				__func__, get_i2s_state_check(), I2S_STATE_CHECK_3);
		return -EFAULT;
	}
#endif
	data = devm_kzalloc(&i2c->dev,
				sizeof(*data),
				GFP_KERNEL);
	if (IS_ERR_OR_NULL(data))
		return -ENOMEM;

	data->regmap = devm_regmap_init_i2c(i2c, &es8156_regmap);
	if (IS_ERR(data->regmap)) {
		ret = PTR_ERR(data->regmap);
		dev_err(&i2c->dev, "Failed to allocate register map: %d\n", ret);
		return ret;
	}

	i2c_set_clientdata(i2c, data);
	data->i2c = i2c;
	data->is24bit = DEFAULT_IS_24BIT_ENABLED;

	//es8156_parse_dt(data);

	//es8156_set_enabled(data, true);

	if (es8156_test_i2c(data) < 0) {
		dev_err(&i2c->dev, "%s: es8156 i2c is failed. please check hard.\n", __func__);
		//return -ENODEV;
	}

	g_es8156 = data;

	es8156_reset(data);
	es8156_init_regs(data);
	es8156_set_dsp_mic_enable(true);
	data->cls_node = class_create(THIS_MODULE, ES8156_NAME);
	if (IS_ERR_OR_NULL(data->cls_node)) {
		dev_err(&i2c->dev, "failed to class_create htfyun-es8156\n");
	} else {
		CLASS_CREATE_FILE(data, gpio);
		CLASS_CREATE_FILE(data, reg);
		CLASS_CREATE_FILE(data, is24bit);
	}

    INIT_DELAYED_WORK(&data->resume, es8156_resume_work);
	return 0;
}

static int es8156_i2c_remove(struct i2c_client *i2c)
{
	struct es8156_priv *data = i2c_get_clientdata(i2c);

	//es8156_set_enabled(data, false);

	if (!IS_ERR(data->cls_node)) {
		CLASS_REMOVE_FILE(data, gpio);
		CLASS_REMOVE_FILE(data, reg);
		CLASS_REMOVE_FILE(data, is24bit);
		class_destroy(data->cls_node);
	}

	g_es8156 = NULL;
	return 0;
}

static void es8156_i2c_shutdown(struct i2c_client *i2c)
{
	//struct es8156_priv *data = dev_get_drvdata(&i2c->dev);
	//es8156_set_enabled(data, false);
}

#ifdef CONFIG_PM
static int es8156_suspend(struct device *dev)
{
    struct es8156_priv * data = g_es8156;
	//es8156_reset(data);
	//es8156_init_regs(data);
	cancel_delayed_work(&data->resume);
	return 0;
}

static int es8156_resume(struct device *dev)
{
	struct es8156_priv * data = g_es8156;
	//es8156_reset(data);
	//es8156_init_regs(data);
	schedule_delayed_work(&data->resume, msecs_to_jiffies(130));
	return 0;
}


static const struct dev_pm_ops es8156_pm_ops = {
	.suspend	= es8156_suspend,
	.resume		= es8156_resume,
};
#endif

static struct i2c_driver es8156_i2c_driver = {
	.driver = {
			.name = ES8156_NAME,
			.owner = THIS_MODULE,
			.of_match_table = of_match_ptr(es8156_of_match),
		#ifdef CONFIG_PM
			.pm	= &es8156_pm_ops,
		#endif
		},
	.probe = es8156_i2c_probe,
	.remove = es8156_i2c_remove,
	.shutdown = es8156_i2c_shutdown,
	.id_table = es8156_i2c_id,
};
static int __init es8156_init(void)
{
	return i2c_add_driver(&es8156_i2c_driver);
}

static void __exit es8156_exit(void)
{
	i2c_del_driver(&es8156_i2c_driver);
}

late_initcall(es8156_init);
module_exit(es8156_exit);

//module_i2c_driver(es8156_i2c_driver);

MODULE_DESCRIPTION("Audio ADC ES8156 driver");
MODULE_AUTHOR("songshitian <songshitian@htfyun.com>");
MODULE_LICENSE("GPL v2");

