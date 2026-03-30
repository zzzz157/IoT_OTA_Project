#include "main.h"
#include "lvgl.h"
#include "MAX30102.h"
#include "Broker.h"
#include "lvgl.h"
#include "lv_port_disp_template.h"
#include "lv_port_indev_template.h"

static lv_obj_t * label_hr_value;
static lv_obj_t * label_spo2_value;

// 1. 创建心率和血氧的 UI 界面
void create_health_ui(void) 
{
    lv_obj_t * scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0); // 纯黑底色

    // ==========================================
    // 上半部分：心率卡片
    // ==========================================
    lv_obj_t * card_hr = lv_obj_create(scr);
    lv_obj_set_size(card_hr, 200, 100);
    lv_obj_align(card_hr, LV_ALIGN_TOP_MID, 0, 25);
    lv_obj_set_style_bg_color(card_hr, lv_color_hex(0x1C1C1E), 0); // 深灰卡片
    lv_obj_set_style_radius(card_hr, 15, 0);                       // 圆角
    lv_obj_set_style_border_width(card_hr, 0, 0);                  // 去除边框
    lv_obj_clear_flag(card_hr, LV_OBJ_FLAG_SCROLLABLE);

    // 心率标题与图标
    lv_obj_t * label_hr_title = lv_label_create(card_hr);
    lv_label_set_text(label_hr_title, "\xE2\x99\xA5 HR");
    lv_obj_set_style_text_color(label_hr_title, lv_color_hex(0xFF2D55), 0); // 红色
    lv_obj_align(label_hr_title, LV_ALIGN_TOP_LEFT, -5, -5);

    // 心率数值
    label_hr_value = lv_label_create(card_hr);
    lv_label_set_text(label_hr_value, "--");
    lv_obj_set_style_text_font(label_hr_value, &lv_font_montserrat_48, 0); // 使用大字体
    lv_obj_set_style_text_color(label_hr_value, lv_color_white(), 0);
    lv_obj_align(label_hr_value, LV_ALIGN_CENTER, -15, 10);

    // 心率单位
    lv_obj_t * label_hr_unit = lv_label_create(card_hr);
    lv_label_set_text(label_hr_unit, "bpm");
    lv_obj_set_style_text_color(label_hr_unit, lv_color_hex(0x8A8A8E), 0);
    lv_obj_align_to(label_hr_unit, label_hr_value, LV_ALIGN_OUT_RIGHT_BOTTOM, 5, -8);

    // ==========================================
    // 下半部分：血氧卡片
    // ==========================================
    lv_obj_t * card_spo2 = lv_obj_create(scr);
    lv_obj_set_size(card_spo2, 200, 100);
    lv_obj_align(card_spo2, LV_ALIGN_BOTTOM_MID, 0, -25);
    lv_obj_set_style_bg_color(card_spo2, lv_color_hex(0x1C1C1E), 0); 
    lv_obj_set_style_radius(card_spo2, 15, 0);
    lv_obj_set_style_border_width(card_spo2, 0, 0);
    lv_obj_clear_flag(card_spo2, LV_OBJ_FLAG_SCROLLABLE);

    // 血氧标题与图标 (使用水滴图标代表血液/血氧)
    lv_obj_t * label_spo2_title = lv_label_create(card_spo2);
    lv_label_set_text(label_spo2_title, LV_SYMBOL_TINT " SpO2");
    lv_obj_set_style_text_color(label_spo2_title, lv_color_hex(0x00F0FF), 0); // 青色
    lv_obj_align(label_spo2_title, LV_ALIGN_TOP_LEFT, -5, -5);

    // 血氧数值
    label_spo2_value = lv_label_create(card_spo2);
    lv_label_set_text(label_spo2_value, "--");
    lv_obj_set_style_text_font(label_spo2_value, &lv_font_montserrat_48, 0); 
    lv_obj_set_style_text_color(label_spo2_value, lv_color_white(), 0);
    lv_obj_align(label_spo2_value, LV_ALIGN_CENTER, -15, 10);

    // 血氧单位
    lv_obj_t * label_spo2_unit = lv_label_create(card_spo2);
    lv_label_set_text(label_spo2_unit, "%");
    lv_obj_set_style_text_color(label_spo2_unit, lv_color_hex(0x8A8A8E), 0);
    lv_obj_align_to(label_spo2_unit, label_spo2_value, LV_ALIGN_OUT_RIGHT_BOTTOM, 5, -8);
}

static QueueHandle_t xLVGLQue_HealthData;
static void update_health_data_cb(lv_timer_t * timer) 
{
    if(label_hr_value && label_spo2_value)
	{
		HealthData_t HrSpo2;
		BaseType_t has_new_data = pdFALSE;
		while(xQueueReceive(xLVGLQue_HealthData, &HrSpo2, 0) == pdTRUE) 
		{
            has_new_data = pdTRUE;
        }
		if(has_new_data == pdTRUE) 
		{
			if(HrSpo2.confidence)
			{
				lv_label_set_text_fmt(label_hr_value, "%d", HrSpo2.HeartRate_Value);
				lv_label_set_text_fmt(label_spo2_value, "%d", HrSpo2.Spo2_Value);
			}
			else
			{
				lv_label_set_text(label_hr_value, "--");
				lv_label_set_text(label_spo2_value, "--");
			}
            
        }
    }
}
void GUI_Task(void* arg)
{
	LOG_DEBUG("LVGL_Task");
	vTaskDelay(50);
	lv_init();
	lv_port_disp_init();
    lv_port_indev_init();
	LOG_DEBUG("LVGL Init Cplt");
	xLVGLQue_HealthData=xQueueCreate(5,sizeof(HealthData_t));
	Broker_Subscribe(TOPIC_HEALTH_DATA,xLVGLQue_HealthData);
	
	create_health_ui();
	LOG_DEBUG("GUI_OK");
	lv_timer_create(update_health_data_cb, 500, NULL);
	
	while(1)
    {
        lv_timer_handler();
        vTaskDelay(5);
    }
}