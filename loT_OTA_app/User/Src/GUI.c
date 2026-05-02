#include "main.h"
#include "lvgl.h"
#include "GUI.h"
#include "MAX30102.h"
#include "Broker.h"
#include "OTA.h"
#include "MQTT.h"
#include "lvgl.h"
#include "lv_port_disp_template.h"
#include "lv_port_indev_template.h"
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"

/* health data */
static lv_obj_t * label_hr_value;
static lv_obj_t * label_spo2_value;
/* ota */
static OTA_Config ota_req;
static OTA_Status ota_status;
static uint8_t is_req_upgrade=0;
static uint8_t is_paused=0;
/* page containers */
static lv_obj_t * home_page;
static lv_obj_t * health_page;
static lv_obj_t * ota_page;
static lv_obj_t * ota_progress_page;

static lv_obj_t * label_ota_status;
static lv_obj_t * ota_progress_bar;
static lv_obj_t * label_ota_percent;
static lv_obj_t * label_ota_available;
static lv_obj_t * btn_ota_pause;
static lv_obj_t * btn_ota_resume;
static lv_obj_t * btn_ota_restart;
static lv_obj_t * btn_ota_return;


/**/
extern EventGroupHandle_t xGlobalEventGroup;
/* ---------- page pool ---------- */
static lv_obj_t * page_pool[MAX_PAGES_SIZE];
static uint8_t page_count;

static void register_page(lv_obj_t * page)
{
    if(page_count < MAX_PAGES_SIZE)
        page_pool[page_count++] = page;
}

static void show_page(lv_obj_t * target)
{
    for (uint8_t i = 0; i < page_count; i++)
        lv_obj_add_flag(page_pool[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(target, LV_OBJ_FLAG_HIDDEN);
}

/* page switch callback — user_data is a pointer-to-pointer
   so it survives the page variable being assigned later */
static void show_page_cb(lv_event_t * e)
{
    lv_obj_t ** page_ptr = lv_event_get_user_data(e);
    show_page(*page_ptr);
}

/* OTA start callback */
static void ota_start_cb(lv_event_t * e)
{
	if(is_req_upgrade==1)
	{
		show_page(ota_progress_page);
		is_paused = 0;
		if(btn_ota_pause)
			lv_obj_clear_flag(btn_ota_pause, LV_OBJ_FLAG_HIDDEN);
		if(btn_ota_resume)
			lv_obj_add_flag(btn_ota_resume, LV_OBJ_FLAG_HIDDEN);
		if(btn_ota_restart)
			lv_obj_add_flag(btn_ota_restart, LV_OBJ_FLAG_HIDDEN);
		Broker_Publish(TOPIC_OTA_DATA,&ota_req);
		is_req_upgrade=0;
	}
    else
	{
		lv_label_set_text(label_ota_available, "No update available.");
	}
}

/* OTA pause callback */
static void ota_pause_cb(lv_event_t * e)
{
	is_paused = 1;
	xEventGroupClearBits(xGlobalEventGroup, EVENT_OTA_NORMALCONTROL);
}

/* OTA resume callback */
static void ota_resume_cb(lv_event_t * e)
{
	is_paused = 0;
	xEventGroupSetBits(xGlobalEventGroup, EVENT_OTA_NORMALCONTROL);
}

/* OTA restart callback */
static void ota_restart_cb(lv_event_t * e)
{
	lv_label_set_text(label_ota_status, "SystemReseting");
	xEventGroupSetBits(xGlobalEventGroup, EVENT_OTA_SYSTEMRESET);
}


extern uint8_t is_have_later_firmware;
/* OTA resume download callback (breakpoint) */
static void ota_resume_download_cb(lv_event_t * e)
{
	if(is_have_later_firmware==0)
	{
		lv_label_set_text(label_ota_available, "No later firmware.");
		return;
	}
	xEventGroupSetBits(xGlobalEventGroup, EVENT_OTA_RESUMEDOWNLOAD);
	show_page(ota_progress_page);
    is_paused = 0;
	if(btn_ota_pause)
        lv_obj_clear_flag(btn_ota_pause, LV_OBJ_FLAG_HIDDEN);
    if(btn_ota_resume)
        lv_obj_add_flag(btn_ota_resume, LV_OBJ_FLAG_HIDDEN);
    if(btn_ota_restart)
        lv_obj_add_flag(btn_ota_restart, LV_OBJ_FLAG_HIDDEN);
}

/* ---------- 1. home page ---------- */
static void create_home_page(void)
{
    home_page = lv_obj_create(lv_scr_act());
    lv_obj_set_size(home_page, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(home_page, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(home_page, 0, 0);
    lv_obj_set_style_pad_all(home_page, 0, 0);
    lv_obj_clear_flag(home_page, LV_OBJ_FLAG_SCROLLABLE);

    /* title */
    lv_obj_t * title = lv_label_create(home_page);
    lv_label_set_text(title, "Smart Health");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    /* HR/SpO2 button */
    lv_obj_t * btn_health = lv_btn_create(home_page);
    lv_obj_set_size(btn_health, 200, 60);
    lv_obj_align(btn_health, LV_ALIGN_CENTER, 0, -40);
    lv_obj_set_style_bg_color(btn_health, lv_color_hex(0xFF2D55), 0);
    lv_obj_set_style_radius(btn_health, 12, 0);

    lv_obj_t * label_btn_health = lv_label_create(btn_health);
    lv_label_set_text(label_btn_health, "\xE2\x99\xA5 HR / SpO2");
    lv_obj_set_style_text_color(label_btn_health, lv_color_white(), 0);
    lv_obj_center(label_btn_health);

    /* OTA button */
    lv_obj_t * btn_ota = lv_btn_create(home_page);
    lv_obj_set_size(btn_ota, 200, 60);
    lv_obj_align(btn_ota, LV_ALIGN_CENTER, 0, 40);
    lv_obj_set_style_bg_color(btn_ota, lv_color_hex(0x007AFF), 0);
    lv_obj_set_style_radius(btn_ota, 12, 0);

    lv_obj_t * label_btn_ota = lv_label_create(btn_ota);
    lv_label_set_text(label_btn_ota, "# OTA Update");
    lv_obj_set_style_text_color(label_btn_ota, lv_color_white(), 0);
    lv_obj_center(label_btn_ota);

    /* button events: v8.2 style, pass target page via user_data */
    lv_obj_add_event_cb(btn_health, show_page_cb, LV_EVENT_CLICKED, &health_page);
    lv_obj_add_event_cb(btn_ota,   show_page_cb, LV_EVENT_CLICKED, &ota_page);

    register_page(home_page);
}

/* ---------- 2. health page (original UI) ---------- */
static void create_health_page(void)
{
    health_page = lv_obj_create(lv_scr_act());
    lv_obj_set_size(health_page, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(health_page, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(health_page, 0, 0);
    lv_obj_set_style_pad_all(health_page, 0, 0);
    lv_obj_clear_flag(health_page, LV_OBJ_FLAG_SCROLLABLE);

    /* back button */
    lv_obj_t * btn_back = lv_btn_create(health_page);
    lv_obj_set_size(btn_back, 60, 30);
    lv_obj_align(btn_back, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x3A3A3C), 0);

    lv_obj_t * label_back = lv_label_create(btn_back);
    lv_label_set_text(label_back, LV_SYMBOL_LEFT " Back");
    lv_obj_center(label_back);

    lv_obj_add_event_cb(btn_back, show_page_cb, LV_EVENT_CLICKED, &home_page);

    /* ---- HR card ---- */
    lv_obj_t * card_hr = lv_obj_create(health_page);
    lv_obj_set_size(card_hr, 200, 100);
    lv_obj_align(card_hr, LV_ALIGN_TOP_MID, 0, 55);
    lv_obj_set_style_bg_color(card_hr, lv_color_hex(0x1C1C1E), 0);
    lv_obj_set_style_radius(card_hr, 15, 0);
    lv_obj_set_style_border_width(card_hr, 0, 0);
    lv_obj_clear_flag(card_hr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * label_hr_title = lv_label_create(card_hr);
    lv_label_set_text(label_hr_title, "\xE2\x99\xA5 HR");
    lv_obj_set_style_text_color(label_hr_title, lv_color_hex(0xFF2D55), 0);
    lv_obj_align(label_hr_title, LV_ALIGN_TOP_LEFT, -5, -5);

    label_hr_value = lv_label_create(card_hr);
    lv_label_set_text(label_hr_value, "--");
    lv_obj_set_style_text_font(label_hr_value, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(label_hr_value, lv_color_white(), 0);
    lv_obj_align(label_hr_value, LV_ALIGN_CENTER, -15, 10);

    lv_obj_t * label_hr_unit = lv_label_create(card_hr);
    lv_label_set_text(label_hr_unit, "bpm");
    lv_obj_set_style_text_color(label_hr_unit, lv_color_hex(0x8A8A8E), 0);
    lv_obj_align_to(label_hr_unit, label_hr_value, LV_ALIGN_OUT_RIGHT_BOTTOM, 5, -8);

    /* ---- SpO2 card ---- */
    lv_obj_t * card_spo2 = lv_obj_create(health_page);
    lv_obj_set_size(card_spo2, 200, 100);
    lv_obj_align_to(card_spo2, card_hr, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);
    lv_obj_set_style_bg_color(card_spo2, lv_color_hex(0x1C1C1E), 0);
    lv_obj_set_style_radius(card_spo2, 15, 0);
    lv_obj_set_style_border_width(card_spo2, 0, 0);
    lv_obj_clear_flag(card_spo2, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * label_spo2_title = lv_label_create(card_spo2);
    lv_label_set_text(label_spo2_title, LV_SYMBOL_TINT " SpO2");
    lv_obj_set_style_text_color(label_spo2_title, lv_color_hex(0x00F0FF), 0);
    lv_obj_align(label_spo2_title, LV_ALIGN_TOP_LEFT, -5, -5);

    label_spo2_value = lv_label_create(card_spo2);
    lv_label_set_text(label_spo2_value, "--");
    lv_obj_set_style_text_font(label_spo2_value, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(label_spo2_value, lv_color_white(), 0);
    lv_obj_align(label_spo2_value, LV_ALIGN_CENTER, -15, 10);

    lv_obj_t * label_spo2_unit = lv_label_create(card_spo2);
    lv_label_set_text(label_spo2_unit, "%");
    lv_obj_set_style_text_color(label_spo2_unit, lv_color_hex(0x8A8A8E), 0);
    lv_obj_align_to(label_spo2_unit, label_spo2_value, LV_ALIGN_OUT_RIGHT_BOTTOM, 5, -8);

    register_page(health_page);
}

/* ---------- 3. OTA page ---------- */
static void create_ota_page(void)
{
    ota_page = lv_obj_create(lv_scr_act());
    lv_obj_set_size(ota_page, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(ota_page, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(ota_page, 0, 0);
    lv_obj_set_style_pad_all(ota_page, 0, 0);
    lv_obj_clear_flag(ota_page, LV_OBJ_FLAG_SCROLLABLE);

    /* back button */
    lv_obj_t * btn_back = lv_btn_create(ota_page);
    lv_obj_set_size(btn_back, 60, 30);
    lv_obj_align(btn_back, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x3A3A3C), 0);
    lv_obj_set_style_radius(btn_back, 8, 0);

    lv_obj_t * label_back = lv_label_create(btn_back);
    lv_label_set_text(label_back, LV_SYMBOL_LEFT " Back");
    lv_obj_center(label_back);

    lv_obj_add_event_cb(btn_back, show_page_cb, LV_EVENT_CLICKED, &home_page);

    /* title */
    lv_obj_t * title = lv_label_create(ota_page);
    lv_label_set_text(title, "OTA Update");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 40);

    /* info text */
    lv_obj_t * info = lv_label_create(ota_page);
    lv_label_set_text(info, "Ensure sufficient battery.\nDo not power off during update.");
    lv_obj_set_style_text_color(info, lv_color_hex(0x8A8A8E), 0);
    lv_obj_set_style_text_font(info, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_align(info, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(info, LV_ALIGN_CENTER, 0, -60);

    /* status label */
    label_ota_available = lv_label_create(ota_page);
    lv_label_set_text(label_ota_available, "");
    lv_obj_set_style_text_color(label_ota_available, lv_color_hex(0x34C759), 0);
    lv_obj_set_style_text_font(label_ota_available, &lv_font_montserrat_14, 0);
    lv_obj_align(label_ota_available, LV_ALIGN_CENTER, 0, -38);

    /* start button */
    lv_obj_t * btn_start = lv_btn_create(ota_page);
    lv_obj_set_size(btn_start, 200, 44);
    lv_obj_align(btn_start, LV_ALIGN_CENTER, 0, 14);
    lv_obj_set_style_bg_color(btn_start, lv_color_hex(0x34C759), 0);
    lv_obj_set_style_radius(btn_start, 12, 0);

    lv_obj_t * label_start = lv_label_create(btn_start);
    lv_label_set_text(label_start, LV_SYMBOL_DOWNLOAD "  Start Update");
    lv_obj_set_style_text_color(label_start, lv_color_white(), 0);
    lv_obj_center(label_start);

    lv_obj_add_event_cb(btn_start, ota_start_cb, LV_EVENT_CLICKED, NULL);

    /* divider */
    lv_obj_t * divider = lv_label_create(ota_page);
    lv_label_set_text(divider, "or");
    lv_obj_set_style_text_color(divider, lv_color_hex(0x8A8A8E), 0);
    lv_obj_set_style_text_font(divider, &lv_font_montserrat_12, 0);
    lv_obj_align(divider, LV_ALIGN_CENTER, 0, 50);

    /* resume download button (breakpoint) */
    lv_obj_t * btn_resume_dl = lv_btn_create(ota_page);
    lv_obj_set_size(btn_resume_dl, 200, 44);
    lv_obj_align(btn_resume_dl, LV_ALIGN_CENTER, 0, 86);
    lv_obj_set_style_bg_color(btn_resume_dl, lv_color_hex(0xFF9500), 0);
    lv_obj_set_style_radius(btn_resume_dl, 12, 0);

    lv_obj_t * label_resume_dl = lv_label_create(btn_resume_dl);
    lv_label_set_text(label_resume_dl, LV_SYMBOL_REFRESH "  Resume Download");
    lv_obj_set_style_text_color(label_resume_dl, lv_color_white(), 0);
    lv_obj_center(label_resume_dl);

    lv_obj_add_event_cb(btn_resume_dl, ota_resume_download_cb, LV_EVENT_CLICKED, NULL);

    register_page(ota_page);
}

/* ---------- 4. OTA progress page ---------- */
static void create_ota_progress_page(void)
{
    ota_progress_page = lv_obj_create(lv_scr_act());
    lv_obj_set_size(ota_progress_page, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(ota_progress_page, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(ota_progress_page, 0, 0);
    lv_obj_set_style_pad_all(ota_progress_page, 0, 0);
    lv_obj_clear_flag(ota_progress_page, LV_OBJ_FLAG_SCROLLABLE);

    /* title */
    lv_obj_t * title = lv_label_create(ota_progress_page);
    lv_label_set_text(title, "OTA Update");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 50);

    /* status text */
    label_ota_status = lv_label_create(ota_progress_page);
    lv_label_set_text(label_ota_status, "Updating...");
    lv_obj_set_style_text_color(label_ota_status, lv_color_hex(0x007AFF), 0);
    lv_obj_set_style_text_font(label_ota_status, &lv_font_montserrat_20, 0);
    lv_obj_align(label_ota_status, LV_ALIGN_CENTER, 0, -40);

    /* progress bar */
    ota_progress_bar = lv_bar_create(ota_progress_page);
    lv_obj_set_size(ota_progress_bar, 200, 15);
    lv_obj_align(ota_progress_bar, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(ota_progress_bar, lv_color_hex(0x3A3A3C), 0);
    lv_obj_set_style_radius(ota_progress_bar, 8, 0);
    lv_bar_set_range(ota_progress_bar, 0, 100);
    lv_bar_set_value(ota_progress_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(ota_progress_bar, lv_color_hex(0x007AFF), LV_PART_INDICATOR);

    /* percentage label */
    label_ota_percent = lv_label_create(ota_progress_page);
    lv_label_set_text(label_ota_percent, "0%");
    lv_obj_set_style_text_color(label_ota_percent, lv_color_hex(0x007AFF), 0);
    lv_obj_set_style_text_font(label_ota_percent, &lv_font_montserrat_20, 0);
    lv_obj_align(label_ota_percent, LV_ALIGN_CENTER, 0, 30);

    /* hint */
    lv_obj_t * hint = lv_label_create(ota_progress_page);
    lv_label_set_text(hint, "Do not power off!");
    lv_obj_set_style_text_color(hint, lv_color_hex(0xFF2D55), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, 55);

    /* pause button */
    btn_ota_pause = lv_btn_create(ota_progress_page);
    lv_obj_set_size(btn_ota_pause, 140, 45);
    lv_obj_align(btn_ota_pause, LV_ALIGN_CENTER, 0, 90);
    lv_obj_set_style_bg_color(btn_ota_pause, lv_color_hex(0xFF9500), 0);
    lv_obj_set_style_radius(btn_ota_pause, 10, 0);
    lv_obj_add_flag(btn_ota_pause, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t * label_pause = lv_label_create(btn_ota_pause);
    lv_label_set_text(label_pause, LV_SYMBOL_PAUSE " Pause");
    lv_obj_set_style_text_color(label_pause, lv_color_white(), 0);
    lv_obj_center(label_pause);
    lv_obj_add_event_cb(btn_ota_pause, ota_pause_cb, LV_EVENT_CLICKED, NULL);

    /* resume button */
    btn_ota_resume = lv_btn_create(ota_progress_page);
    lv_obj_set_size(btn_ota_resume, 140, 45);
    lv_obj_align(btn_ota_resume, LV_ALIGN_CENTER, 0, 90);
    lv_obj_set_style_bg_color(btn_ota_resume, lv_color_hex(0x34C759), 0);
    lv_obj_set_style_radius(btn_ota_resume, 10, 0);
    lv_obj_add_flag(btn_ota_resume, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t * label_resume = lv_label_create(btn_ota_resume);
    lv_label_set_text(label_resume, LV_SYMBOL_PLAY " Resume");
    lv_obj_set_style_text_color(label_resume, lv_color_white(), 0);
    lv_obj_center(label_resume);
    lv_obj_add_event_cb(btn_ota_resume, ota_resume_cb, LV_EVENT_CLICKED, NULL);

    /* restart button */
    btn_ota_restart = lv_btn_create(ota_progress_page);
    lv_obj_set_size(btn_ota_restart, 140, 45);
    lv_obj_align(btn_ota_restart, LV_ALIGN_CENTER, 0, 90);
    lv_obj_set_style_bg_color(btn_ota_restart, lv_color_hex(0x007AFF), 0);
    lv_obj_set_style_radius(btn_ota_restart, 10, 0);
    lv_obj_add_flag(btn_ota_restart, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t * label_restart = lv_label_create(btn_ota_restart);
    lv_label_set_text(label_restart, LV_SYMBOL_REFRESH " Restart");
    lv_obj_set_style_text_color(label_restart, lv_color_white(), 0);
    lv_obj_center(label_restart);
    lv_obj_add_event_cb(btn_ota_restart, ota_restart_cb, LV_EVENT_CLICKED, NULL);

    /* return button (shown on failure) */
    btn_ota_return = lv_btn_create(ota_progress_page);
    lv_obj_set_size(btn_ota_return, 140, 45);
    lv_obj_align(btn_ota_return, LV_ALIGN_CENTER, 0, 90);
    lv_obj_set_style_bg_color(btn_ota_return, lv_color_hex(0x8A8A8E), 0);
    lv_obj_set_style_radius(btn_ota_return, 10, 0);
    lv_obj_add_flag(btn_ota_return, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t * label_return = lv_label_create(btn_ota_return);
    lv_label_set_text(label_return, LV_SYMBOL_LEFT " Return");
    lv_obj_set_style_text_color(label_return, lv_color_white(), 0);
    lv_obj_center(label_return);
    lv_obj_add_event_cb(btn_ota_return, show_page_cb, LV_EVENT_CLICKED, &ota_page);

    register_page(ota_progress_page);
}

/* ---------- create all UI ---------- */
static void create_all_ui(void)
{
    lv_obj_t * scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);

    /* create sub-pages first; home page buttons reference their pointers */
    create_health_page();
    create_ota_page();
    create_ota_progress_page();
    create_home_page();

    /* show home page by default */
    show_page(home_page);
}

/* ---------- health data update ---------- */
static QueueHandle_t xLVGLQue_HealthData;
static QueueHandle_t xLVGLQue_OtaReq;
static QueueHandle_t xLVGLQue_OtaProgress;
static QueueHandle_t xLVGLQue_OtaStatus;
static void update_health_data_cb(lv_timer_t * timer)
{
	/* recv heal data */
    if (label_hr_value && label_spo2_value)
    {
        HealthData_t HrSpo2;
        BaseType_t has_new_data = pdFALSE;
        while (xQueueReceive(xLVGLQue_HealthData, &HrSpo2, 0) == pdTRUE)
        {
            has_new_data = pdTRUE;
        }
        if (has_new_data == pdTRUE)
        {
            if (HrSpo2.confidence)
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
	/* recv ota */
	if(ota_page&&ota_progress_page)
	{
		/* recv ota req */
		BaseType_t has_ota_req = pdFALSE;
		while (xQueueReceive(xLVGLQue_OtaReq, &ota_req, 0) == pdTRUE)
        {
            has_ota_req = pdTRUE;
        }
		if(has_ota_req==pdTRUE)
		{
			is_req_upgrade=1;
			if(label_ota_available)
			{
				lv_label_set_text(label_ota_available, "Update Available!");
			}
		}
		/* get ota status */
		while(xQueueReceive(xLVGLQue_OtaStatus, &ota_status, 0)==pdTRUE);
	}
	/* updata ota progress */
	if(is_paused==0)
	{
		BaseType_t has_new_progress = pdFALSE;
		uint8_t percent;
		while (xQueueReceive(xLVGLQue_OtaProgress, &percent, 0) == pdTRUE)
        {
            has_new_progress = pdTRUE;
        }
		if(has_new_progress==pdTRUE)
		{
			if(ota_progress_bar)
			{
				lv_bar_set_value(ota_progress_bar, percent, LV_ANIM_ON);
			}
			if(label_ota_percent)
			{
				lv_label_set_text_fmt(label_ota_percent, "%d%%", percent);
			}
			if(percent >= 99)
			{
				if(btn_ota_pause)
					lv_obj_add_flag(btn_ota_pause, LV_OBJ_FLAG_HIDDEN);
				if(btn_ota_resume)
					lv_obj_add_flag(btn_ota_resume, LV_OBJ_FLAG_HIDDEN);
			}
			else
			{
				if(label_ota_status)
					lv_label_set_text(label_ota_status, "Updating...");
				if(btn_ota_pause)
					lv_obj_clear_flag(btn_ota_pause, LV_OBJ_FLAG_HIDDEN);
				if(btn_ota_resume)
					lv_obj_add_flag(btn_ota_resume, LV_OBJ_FLAG_HIDDEN);
				if(btn_ota_restart)
					lv_obj_add_flag(btn_ota_restart, LV_OBJ_FLAG_HIDDEN);
			}
		}
	}
	else
	{
		if(label_ota_status)
			lv_label_set_text(label_ota_status, "Paused");
		if(btn_ota_pause)
			lv_obj_add_flag(btn_ota_pause, LV_OBJ_FLAG_HIDDEN);
		if(btn_ota_resume)
			lv_obj_clear_flag(btn_ota_resume, LV_OBJ_FLAG_HIDDEN);
		if(btn_ota_restart)
			lv_obj_add_flag(btn_ota_restart, LV_OBJ_FLAG_HIDDEN);
	}
	/* deal ota status */
	switch(ota_status)
	{
		case OTA_STATUS_DOWNLOADING:
		{
			break;
		}
		case OTA_STATUS_SUCCESS:
		{
			if(label_ota_status)
			{
				lv_label_set_text(label_ota_status, "Update Success!");
				lv_obj_set_style_text_color(label_ota_status, lv_color_hex(0x34C759), 0);
				if(btn_ota_restart)
					lv_obj_clear_flag(btn_ota_restart, LV_OBJ_FLAG_HIDDEN);
				if(btn_ota_return)
					lv_obj_add_flag(btn_ota_return, LV_OBJ_FLAG_HIDDEN);
				ota_status=OTA_STATUS_IDLE;
			}
			break;
		}
		case OTA_STATUS_FAILED:
		{
			if(label_ota_status)
			{
				lv_label_set_text(label_ota_status, "Update Failed!");
				lv_obj_set_style_text_color(label_ota_status, lv_color_hex(0xFF2D55), 0);
				if(btn_ota_restart)
					lv_obj_add_flag(btn_ota_restart, LV_OBJ_FLAG_HIDDEN);
				if(btn_ota_return)
					lv_obj_clear_flag(btn_ota_return, LV_OBJ_FLAG_HIDDEN);
				ota_status=OTA_STATUS_IDLE;
			}
			break;
		}
		default:
		{
			
			break;
		}
	}
}

void GUI_Task(void *pvParameters)
{
    LOG_DEBUG("LVGL_Task");
    vTaskDelay(50);
	/* init lvgl */
    lv_init();
    lv_port_disp_init();
    lv_port_indev_init();
    LOG_DEBUG("LVGL Init Cplt");
	xEventGroupSetBits(xGlobalEventGroup, EVENT_OTA_NORMALCONTROL);
	/* Subscribe health data */
    xLVGLQue_HealthData = xQueueCreate(5, sizeof(HealthData_t));
    Broker_Subscribe(TOPIC_HEALTH_DATA, xLVGLQue_HealthData);
	/* Subscribe ota req */
	xLVGLQue_OtaReq = xQueueCreate(3, sizeof(OTA_Config));
	Broker_Subscribe(TOPIC_OTA_REQ, xLVGLQue_OtaReq);
	/* Subscribe ota upgrade progress */
	xLVGLQue_OtaProgress = xQueueCreate(3, sizeof(uint8_t));
	Broker_Subscribe(TOPIC_OTA_PROGRESS, xLVGLQue_OtaProgress);
	/* Subscribe ota status */
	xLVGLQue_OtaStatus = xQueueCreate(3, sizeof(OTA_Status));
	Broker_Subscribe(TOPIC_OTA_STATUS, xLVGLQue_OtaStatus);
	/* lvgl create page */
    create_all_ui();
    lv_timer_create(update_health_data_cb, 500, NULL);
	LOG_DEBUG("GUI Init Success!");
    while (1)
    {
        lv_timer_handler();
        vTaskDelay(5);
    }
}
