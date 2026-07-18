/*
 * @Author: xiaozhi 
 * @Date: 2024-09-24 23:31:09 
 * @Last Modified by: xiaozhi
 * @Last Modified time: 2024-10-08 18:56:25
 */
#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "color_conf.h"
#include "image_conf.h"
#include "font_conf.h"
#include "page_conf.h"
#include "device_data.h"
#include "page_pulldown_view.h"
#include "em_hal_brightness.h"
#include "em_hal_audio.h"
#include "em_hal_system.h"
#include "music_conf.h"
#include "http_manager.h"

static lv_style_t com_style;

static system_setting_info_t system_setting_info_list[] = {
    {SYSTEM_SETTING_BACKLIGHT,"亮度",GET_IMAGE_PATH("icon_brightness.png"),50,NULL},
    {SYSTEM_SETTING_VOLUME,"音量",GET_IMAGE_PATH("icon_volume.png"),30,NULL},
};

static lv_obj_t *city_roller_obj = NULL;
static uint16_t city_roller_index = 2;
static const char *city_keys[] = {
    "beijing", "shanghai", "guangzhou", "shenzhen", "tianjin", "chongqing",
    "chengdu", "wuhan", "nanjing", "hangzhou", "WTTDPCGXTWUS", "qingdao",
    "dalian", "ningbo", "xiamen", "fuzhou", "changsha", "nanning"
};

#define SETTINGS_LEFT_X      100
#define SETTINGS_TOP_GAP     40
#define SETTINGS_ITEM_GAP    20
#define CITY_RIGHT_GAP       100
#define CITY_TITLE_GAP       16
#define CITY_LABEL_ROLLER_GAP 0
#define CITY_ROLLER_BUTTON_GAP 0
#define CITY_ROW_HEIGHT      310
#define SETTING_ROW_HEIGHT   66

static void com_style_init(){
    lv_style_init(&com_style);
    if(lv_style_is_empty(&com_style) == false)
        lv_style_reset(&com_style);
    lv_style_set_bg_color(&com_style,APP_COLOR_BLACK);
    lv_style_set_radius(&com_style,0);
    lv_style_set_border_width(&com_style,0);
    lv_style_set_pad_all(&com_style,0);
}

static void obj_font_set(lv_obj_t *obj,int type, uint16_t weight){
    lv_font_t* font = get_font(type, weight);
    if(font != NULL)
        lv_obj_set_style_text_font(obj, font, 0);
}

static void make_layout_transparent(lv_obj_t *obj)
{
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

static void back_btn_click_event_cb(lv_event_t * e){
    delete_current_page(&com_style);
    init_page_main();
}

static void slider_release_cb(lv_event_t * e){
    lv_obj_t * slider = lv_event_get_target(e);
    char* menu_name  = (char *)lv_event_get_user_data(e);
    device_state_t* device_state = get_device_state();
    if(strstr(menu_name,"亮度") != NULL){
        em_hal_brightness_set_value(lv_slider_get_value(slider)+5);
        device_state->brightness_value  = lv_slider_get_value(slider);
    }else if(strstr(menu_name,"音量") != NULL){
        em_set_audio_vol(lv_slider_get_value(slider));
        start_play_audio_async(GET_MUSIC_PATH("audio_msg.wav"));
        device_state->volume_value  = lv_slider_get_value(slider);
    }
    device_param_write();
}

static void slider_event_cb(lv_event_t * e)
{
    lv_obj_t * slider = lv_event_get_target(e);
    char buf[8];
    lv_snprintf(buf, sizeof(buf), "%d%%", (int)lv_slider_get_value(slider));
    char* menu_name  = (char *)lv_event_get_user_data(e);
    device_state_t* device_state = get_device_state();
    if(strstr(menu_name,"亮度") != NULL){
        lv_label_set_text(system_setting_info_list[SYSTEM_SETTING_BACKLIGHT].label, buf);
        em_hal_brightness_set_value(lv_slider_get_value(slider)+5);
    }else if(strstr(menu_name,"音量") != NULL){
        lv_label_set_text(system_setting_info_list[SYSTEM_SETTING_VOLUME].label, buf);
    }
}

static void btn_click_event_cb(lv_event_t * e){
    lv_event_code_t code = lv_event_get_code(e);
    device_state_t* device_state = get_device_state();
    if(code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * swbtn = lv_event_get_target(e);
        SWITCH_STATE_E state = OFF;
        if(lv_obj_has_state(swbtn, LV_STATE_CHECKED)){
            state = ON;
            printf("sw btn open\n");
            device_state->is_disp_orientation = true;
        }else{
            state = OFF;
            printf("sw btn close\n");
            device_state->is_disp_orientation = false;
        }
    }
    device_param_write();
}

static void select_btn_click_event_cb(lv_event_t * e){
    em_hal_reboot();
}

static void sync_city_roller_index(void)
{
    device_state_t *device_state = get_device_state();
    size_t length = sizeof(city_keys) / sizeof(city_keys[0]);

    for(size_t i = 0;i < length;i ++){
        if(strcmp(device_state->weather_city, city_keys[i]) == 0){
            city_roller_index = (uint16_t)i;
            return;
        }
    }
    city_roller_index = 2;
}

static void city_roller_event_handler(lv_event_t *e)
{
    if(lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        city_roller_index = lv_roller_get_selected(lv_event_get_target(e));
    }
}

static void city_select_btn_click_event_cb(lv_event_t * e)
{
    (void)e;
    device_state_t *device_state = get_device_state();
    size_t length = sizeof(city_keys) / sizeof(city_keys[0]);

    if(city_roller_index >= length){
        city_roller_index = 2;
    }

    snprintf(device_state->weather_city,
             sizeof(device_state->weather_city),
             "%s",
             city_keys[city_roller_index]);
    printf("city roller index = %d %s\n", city_roller_index, device_state->weather_city);
    http_get_weather_async(WEATHER_KEY, device_state->weather_city);
    device_param_write();
}

static lv_obj_t * init_title_view(lv_obj_t *parent){
    lv_obj_t * cont = lv_obj_create(parent);
    lv_obj_set_size(cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_add_style(cont, &com_style, 0);
    lv_obj_set_align(cont,LV_ALIGN_TOP_MID);
    lv_obj_add_flag(cont,LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *back_img = lv_img_create(cont);
    lv_img_set_src(back_img,GET_IMAGE_PATH("icon_back.png"));
    lv_obj_set_align(back_img,LV_ALIGN_TOP_LEFT);
    lv_obj_set_style_pad_left(back_img,20,LV_PART_MAIN);
    lv_obj_set_style_pad_top(back_img,20,LV_PART_MAIN);

    lv_obj_t *menu_img = lv_img_create(cont);
    lv_img_set_src(menu_img,GET_IMAGE_PATH("icon_setting.png"));
    lv_obj_set_align(menu_img,LV_ALIGN_TOP_LEFT);
    lv_obj_set_style_pad_top(menu_img,20,LV_PART_MAIN);
    lv_obj_align_to(menu_img,back_img,LV_ALIGN_OUT_RIGHT_MID,20,0);

    lv_obj_t *title = lv_label_create(cont);
    obj_font_set(title,FONT_TYPE_LETTER, FONT_SIZE_TEXT_1);
    lv_label_set_text(title,"系统设置");
    lv_obj_set_style_text_color(title,APP_COLOR_WHITE,0);
    lv_obj_align_to(title,menu_img,LV_ALIGN_OUT_RIGHT_MID,20,3);

    lv_obj_add_event_cb(cont,back_btn_click_event_cb,LV_EVENT_CLICKED,NULL);

    return cont;
}

static lv_obj_t *init_scroll_item_view(lv_obj_t *parent,SYSTEM_SETTING_TYPE_E type){
    lv_obj_t * cont = lv_obj_create(parent);
    lv_obj_set_size(cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_add_style(cont, &com_style, 0);
    lv_obj_set_flex_flow(cont,LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(cont,30,0);
    lv_obj_set_style_bg_opa(cont,LV_OPA_0,0);

    lv_obj_t *icon = lv_img_create(cont);
    lv_img_set_src(icon,system_setting_info_list[type].img_url);
    lv_obj_set_align(icon,LV_ALIGN_LEFT_MID);

    lv_obj_t *title_label = lv_label_create(cont);
    obj_font_set(title_label,FONT_TYPE_LETTER, FONT_SIZE_TEXT_2);
    lv_label_set_text(title_label,system_setting_info_list[type].name);
    lv_obj_set_style_text_color(title_label,APP_COLOR_WHITE,0);
    lv_obj_set_style_pad_top(title_label,-10,LV_PART_MAIN);

    lv_obj_t * slider = lv_slider_create(cont);
    lv_slider_set_value(slider,system_setting_info_list[type].value,LV_ANIM_OFF);
    lv_slider_set_range(slider, 0, 100);
    lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, (void *)system_setting_info_list[type].name);
    lv_obj_add_event_cb(slider,slider_release_cb,LV_EVENT_RELEASED, (void *)system_setting_info_list[type].name);

    system_setting_info_list[type].label = lv_label_create(cont);
    obj_font_set(system_setting_info_list[type].label,FONT_TYPE_LETTER, FONT_SIZE_TEXT_2);
    lv_label_set_text_fmt(system_setting_info_list[type].label,"%d%%",system_setting_info_list[type].value);
    lv_obj_set_style_text_color(system_setting_info_list[type].label,APP_COLOR_WHITE,0);
    lv_obj_set_style_pad_top(system_setting_info_list[type].label,-10,LV_PART_MAIN);

    return cont;
}

static lv_obj_t * init_item_btn_view(lv_obj_t *parent,char *name_str,lv_state_t state){
    lv_obj_t * cont = lv_obj_create(parent);
    lv_obj_set_size(cont, LV_SIZE_CONTENT,SETTING_ROW_HEIGHT);
    lv_obj_add_style(cont, &com_style, 0);
    lv_obj_set_align(cont,LV_ALIGN_TOP_MID);
    lv_obj_set_style_bg_opa(cont,LV_OPA_0,0);
    make_layout_transparent(cont);

    lv_obj_t *title = lv_label_create(cont);
    obj_font_set(title,FONT_TYPE_LETTER, FONT_SIZE_TEXT_1);
    lv_label_set_text(title,name_str);
    lv_obj_set_style_text_color(title,APP_COLOR_WHITE,0);
    lv_obj_set_align(title,LV_ALIGN_LEFT_MID);

    lv_obj_t* sw = lv_switch_create(cont);
    lv_obj_set_size(sw,80,35);
    lv_obj_align_to(sw,title,LV_ALIGN_OUT_RIGHT_MID,20,8);
    //选中状态
	lv_obj_set_style_border_width(sw, 2, LV_PART_MAIN);
	lv_obj_set_style_bg_color(sw, APP_COLOR_BUTTON_DEFALUT, LV_PART_INDICATOR | LV_STATE_CHECKED);
	lv_obj_set_style_bg_opa(sw, LV_OPA_100, LV_PART_INDICATOR | LV_STATE_CHECKED);
	lv_obj_set_style_bg_color(sw, APP_COLOR_WHITE, LV_PART_KNOB | LV_STATE_CHECKED);
	lv_obj_set_style_bg_opa(sw, LV_OPA_100, LV_PART_KNOB | LV_STATE_CHECKED);
	lv_obj_set_style_pad_all(sw, -5, LV_PART_KNOB | LV_STATE_CHECKED);
	lv_obj_set_style_border_color(sw, APP_COLOR_BUTTON_DEFALUT, LV_PART_MAIN | LV_STATE_CHECKED);
	lv_obj_set_style_border_opa(sw, LV_OPA_100, LV_PART_MAIN | LV_STATE_CHECKED);
	//未选中状态
    lv_obj_set_style_bg_color(sw, lv_color_hex(0x999999), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(sw, LV_OPA_10, LV_PART_MAIN);
	lv_obj_set_style_border_color(sw, lv_color_hex(0x999999), LV_PART_MAIN);
	lv_obj_set_style_border_opa(sw, LV_OPA_100, LV_PART_MAIN);
	lv_obj_set_style_bg_color(sw, lv_color_hex(0x999999), LV_PART_KNOB);
	lv_obj_set_style_bg_opa(sw, LV_OPA_100, LV_PART_KNOB);
	lv_obj_set_style_pad_all(sw, -10, LV_PART_KNOB);
    lv_obj_add_flag(sw, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_clear_state(sw,LV_STATE_FOCUS_KEY);
    lv_obj_add_event_cb(sw, btn_click_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    lv_obj_add_state(sw,state);

    return cont;
}

static lv_obj_t * init_select_btn(lv_obj_t *parent){
    lv_obj_t * btn = lv_btn_create(parent);
    lv_obj_add_style(btn,&com_style,0);
    lv_obj_set_size(btn,171,66);
    lv_obj_clear_state(btn,LV_STATE_FOCUS_KEY);
    lv_obj_set_style_border_width(btn, 0,0);
    lv_obj_set_style_shadow_width(btn, 0,0);
    lv_obj_set_style_radius(btn,35,0);
    lv_obj_set_style_bg_color(btn,APP_COLOR_BUTTON_DEFALUT,0);
    // lv_obj_set_style_opa(btn,LV_OPA_80,LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn,select_btn_click_event_cb,LV_EVENT_CLICKED,NULL);

    lv_obj_t  * btn_label = lv_label_create(btn);
    obj_font_set(btn_label,FONT_TYPE_CN, FONT_SIZE_TEXT_1);
    lv_obj_set_style_text_color(btn_label,APP_COLOR_WHITE,0);
    lv_label_set_text(btn_label,"点我重启");
    lv_obj_align(btn_label,LV_ALIGN_CENTER,0,-5);

    return btn;
}

static lv_obj_t * init_city_select_btn(lv_obj_t *parent){
    lv_obj_t * btn = lv_btn_create(parent);
    lv_obj_add_style(btn,&com_style,0);
    lv_obj_set_size(btn,171,66);
    lv_obj_clear_state(btn,LV_STATE_FOCUS_KEY);
    lv_obj_set_style_border_width(btn, 0,0);
    lv_obj_set_style_shadow_width(btn, 0,0);
    lv_obj_set_style_radius(btn,35,0);
    lv_obj_set_style_bg_color(btn,APP_COLOR_BUTTON_DEFALUT,0);
    lv_obj_add_event_cb(btn,city_select_btn_click_event_cb,LV_EVENT_CLICKED,NULL);

    lv_obj_t  * btn_label = lv_label_create(btn);
    obj_font_set(btn_label,FONT_TYPE_CN, FONT_SIZE_TEXT_1);
    lv_obj_set_style_text_color(btn_label,APP_COLOR_WHITE,0);
    lv_label_set_text(btn_label,"确定更新城市");
    lv_obj_align(btn_label,LV_ALIGN_CENTER,0,-5);

    return btn;
}

static lv_obj_t * init_city_setting_view(lv_obj_t *parent){
    lv_obj_t * cont = lv_obj_create(parent);
    lv_obj_set_size(cont, LV_SIZE_CONTENT, CITY_ROW_HEIGHT);
    lv_obj_add_style(cont, &com_style, 0);
    lv_obj_set_style_bg_opa(cont,LV_OPA_0,0);
    make_layout_transparent(cont);
    lv_obj_clear_flag(cont,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(cont,LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(cont,0,0);

    lv_obj_t *title_row = lv_obj_create(cont);
    lv_obj_set_size(title_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_add_style(title_row, &com_style, 0);
    lv_obj_set_style_bg_opa(title_row,LV_OPA_0,0);
    make_layout_transparent(title_row);
    lv_obj_clear_flag(title_row,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(title_row,LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(title_row,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(title_row,CITY_TITLE_GAP,0);

    lv_obj_t *setting_img = lv_img_create(title_row);
    lv_img_set_src(setting_img,GET_IMAGE_PATH("icon_city.png"));
    lv_obj_set_style_translate_y(setting_img, 5, 0);

    lv_obj_t *title = lv_label_create(title_row);
    obj_font_set(title,FONT_TYPE_LETTER, FONT_SIZE_TEXT_1);
    lv_label_set_text(title,"城市：");
    lv_obj_set_style_text_color(title,APP_COLOR_WHITE,0);

    lv_obj_t *label_spacer = lv_obj_create(cont);
    lv_obj_set_size(label_spacer, CITY_LABEL_ROLLER_GAP, 1);
    lv_obj_add_style(label_spacer, &com_style, 0);
    make_layout_transparent(label_spacer);

    city_roller_obj  = lv_roller_create(cont);
    lv_obj_set_style_border_width(city_roller_obj,0,0);
    lv_obj_set_style_text_color(city_roller_obj,APP_COLOR_WHITE,0);
    lv_obj_set_style_text_color(city_roller_obj,APP_COLOR_WHITE,LV_PART_SELECTED);
    lv_obj_set_style_bg_color(city_roller_obj,lv_color_black(),0);
    lv_obj_set_style_bg_opa(city_roller_obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(city_roller_obj, LV_OPA_TRANSP, LV_PART_SELECTED);
    lv_obj_set_style_border_width(city_roller_obj, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(city_roller_obj, 0, LV_PART_MAIN);
    lv_obj_set_style_text_align(city_roller_obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_align(city_roller_obj, LV_TEXT_ALIGN_CENTER, LV_PART_SELECTED);
    lv_roller_set_options(city_roller_obj,
                        "北京\n上海\n广州\n深圳\n天津\n重庆\n成都\n武汉\n南京\n杭州\n苏州\n青岛\n大连\n宁波\n厦门\n福州\n长沙\n南宁",
                        LV_ROLLER_MODE_NORMAL);
    lv_obj_add_event_cb(city_roller_obj, city_roller_event_handler, LV_EVENT_ALL, NULL);
    lv_font_t* font_select = get_font(FONT_TYPE_CN_LIGHT, FONT_SIZE_TITLE_2);
    if(font_select != NULL)
        lv_obj_set_style_text_font(city_roller_obj, font_select, LV_PART_SELECTED);
    lv_font_t* un_font_select = get_font(FONT_TYPE_CN_LIGHT, FONT_SIZE_TEXT_1);
	if(un_font_select != NULL)
		lv_obj_set_style_text_font(city_roller_obj, un_font_select, 0);
    lv_obj_set_style_text_line_space(city_roller_obj,8,0);
    lv_obj_set_height(city_roller_obj,310);
    lv_obj_set_width(city_roller_obj,250);
    lv_roller_set_visible_row_count(city_roller_obj, 5);
    lv_roller_set_selected(city_roller_obj,city_roller_index,LV_ANIM_OFF);
    lv_obj_clear_state(city_roller_obj,LV_STATE_FOCUS_KEY);

    lv_obj_t *button_spacer = lv_obj_create(cont);
    lv_obj_set_size(button_spacer, CITY_ROLLER_BUTTON_GAP, 1);
    lv_obj_add_style(button_spacer, &com_style, 0);
    make_layout_transparent(button_spacer);

    init_city_select_btn(cont);

    return cont;
}

void init_page_system_setting()
{
    com_style_init();
    lv_obj_t * cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
    lv_obj_center(cont);
    lv_obj_add_style(cont, &com_style, 0);

    device_state_t* device_state = get_device_state();
    system_setting_info_list[SYSTEM_SETTING_BACKLIGHT].value = device_state->brightness_value;
    system_setting_info_list[SYSTEM_SETTING_VOLUME].value = device_state->volume_value;

    // lv_obj_t *bg_img = lv_img_create(cont);
    // lv_img_set_src(bg_img,GET_IMAGE_PATH("bg_1.png"));
    // lv_obj_align(bg_img,LV_ALIGN_RIGHT_MID,-2,0);

    lv_obj_t *title_view =  init_title_view(cont);
    sync_city_roller_index();

    lv_obj_t *left_panel = lv_obj_create(cont);
    lv_obj_set_size(left_panel, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_add_style(left_panel, &com_style, 0);
    lv_obj_set_style_bg_opa(left_panel,LV_OPA_0,0);
    make_layout_transparent(left_panel);
    lv_obj_set_flex_flow(left_panel,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(left_panel,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(left_panel,SETTINGS_ITEM_GAP,0);
    lv_obj_align_to(left_panel,title_view,LV_ALIGN_OUT_BOTTOM_LEFT,SETTINGS_LEFT_X,SETTINGS_TOP_GAP);

    lv_obj_t* backlight_view = init_scroll_item_view(left_panel,SYSTEM_SETTING_BACKLIGHT);
    (void)backlight_view;
    
    lv_obj_t* volume_view = init_scroll_item_view(left_panel,SYSTEM_SETTING_VOLUME);
    (void)volume_view;

    lv_obj_t *button_row = lv_obj_create(left_panel);
    lv_obj_set_size(button_row, LV_SIZE_CONTENT, SETTING_ROW_HEIGHT);
    lv_obj_add_style(button_row, &com_style, 0);
    make_layout_transparent(button_row);
    lv_obj_set_flex_flow(button_row,LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(button_row,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(button_row,SETTINGS_ITEM_GAP,0);

    lv_obj_t* orientation_btn = init_item_btn_view(button_row,"界面旋转(设置后重启生效)",device_state->is_disp_orientation);
    (void)orientation_btn;
    lv_obj_t * btn = init_select_btn(button_row);
    (void)btn;

    lv_obj_t *city_view = init_city_setting_view(cont);
    lv_obj_update_layout(cont);
    lv_obj_update_layout(left_panel);
    lv_obj_update_layout(city_view);
    lv_obj_align_to(city_view,left_panel,LV_ALIGN_OUT_RIGHT_MID,CITY_RIGHT_GAP,0);
    lv_obj_set_y(city_view,(lv_obj_get_height(cont) - lv_obj_get_height(city_view)) / 2);
}
