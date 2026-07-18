/*
 * @Author: xiaozhi 
 * @Date: 2024-09-24 23:31:09 
 * @Last Modified by: xiaozhi
 * @Last Modified time: 2024-09-26 01:46:05
 */
#include <stdio.h>
#include "lvgl.h"
#include "color_conf.h"
#include "image_conf.h"
#include "font_conf.h"
#include "page_conf.h"
#include "device_data.h"
#include "gateway_state.h"
#include "lv_clock.h"
#include "http_manager.h"

#include <time.h>
#include <sys/time.h>
#include <string.h>

static lv_style_t com_style;

static lv_obj_t * page = NULL;
static lv_obj_t * time_label = NULL;
static lv_obj_t * weather_label = NULL;
static TIME_SHOW_TYPE_E page_type = TIME_TYPE_1;
static int clock_type = 0;
static time_t timep;
static struct tm time_temp;
static char time_str[20];

#define SENSOR_GAUGE_SIZE 130
#define SENSOR_GAUGE_ITEM_WIDTH 150
#define SENSOR_GAUGE_ITEM_HEIGHT 180
#define CLOCK_ITEM_WIDTH 300
#define CLOCK_ITEM_HEIGHT 215

static lv_clock_t lv_clock0;
static lv_clock_t lv_clock1;
static lv_clock_t lv_clock2;
static lv_clock_t *active_clock = NULL;

typedef struct{
    bool has_ap;
    bool has_icm;
    bool offline;
    bool abnormal;
    int lux;
    int proximity;
    int ir;
    float ax;
    float ay;
    float az;
    float gx;
    float gy;
    float gz;
    bool stm32_online;
    bool dht11_ok;
    int dht11_temperature;
    int dht11_humidity;
    char stm32_app_version[16];
    uint32_t stm32_counter;
    char alert[128];
}sensor_status_t;

typedef struct{
    lv_obj_t *arc;
    lv_obj_t *value_label;
    lv_obj_t *unit_label;
    lv_obj_t *title_label;
    bool cache_valid;
    int cached_percent;
    uint32_t cached_color;
    char cached_value[32];
    char cached_unit[8];
}sensor_gauge_view_t;

static sensor_gauge_view_t light_gauge;
static sensor_gauge_view_t temperature_gauge;
static sensor_gauge_view_t humidity_gauge;
static sensor_gauge_view_t proximity_gauge;
static lv_obj_t * node_status_label = NULL;
static bool sensor_warning_reported = false;

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

static float sensor_abs(float value)
{
    return value < 0 ? -value : value;
}

static void make_sleep_obj_transparent(lv_obj_t *obj)
{
    lv_obj_set_style_bg_opa(obj,LV_OPA_TRANSP,LV_PART_MAIN);
    lv_obj_set_style_border_width(obj,0,LV_PART_MAIN);
    lv_obj_set_style_shadow_width(obj,0,LV_PART_MAIN);
    lv_obj_clear_flag(obj,LV_OBJ_FLAG_SCROLLABLE);
}

static void read_sensor_status(sensor_status_t *status)
{
    const gateway_state_t *state = gateway_state_get();

    memset(status,0,sizeof(*status));
    if(!gateway_state_imx6ull_online(state)){
        status->offline = true;
        strcpy(status->alert,"Gateway offline");
        return;
    }

    status->has_ap = state->ap3216c_ok ? true : false;
    status->has_icm = state->icm20608_ok ? true : false;
    status->lux = state->als;
    status->ir = state->ir;
    status->proximity = state->ps;
    status->ax = state->accel_x;
    status->ay = state->accel_y;
    status->az = state->accel_z;
    status->gx = state->gyro_x;
    status->gy = state->gyro_y;
    status->gz = state->gyro_z;
    status->stm32_online = gateway_state_stm32_online(state) ? true : false;
    status->dht11_ok = state->dht11_ok ? true : false;
    status->dht11_temperature = state->dht11_temperature;
    status->dht11_humidity = state->dht11_humidity;
    status->stm32_counter = state->stm32_counter;
    strncpy(status->stm32_app_version,state->stm32_app_version,
            sizeof(status->stm32_app_version) - 1);
    status->stm32_app_version[sizeof(status->stm32_app_version) - 1] = '\0';

    if(!state->ap3216c_ok){
        status->abnormal = true;
        strcpy(status->alert,"AP3216C data abnormal");
    }else if(!state->icm20608_ok){
        status->abnormal = true;
        strcpy(status->alert,"ICM20608 data abnormal");
    }else if(status->proximity > 500){
        status->abnormal = true;
        strcpy(status->alert,"AP3216C proximity abnormal");
    }else if(sensor_abs(status->gx) > 200 || sensor_abs(status->gy) > 200 ||
             sensor_abs(status->gz) > 200 || sensor_abs(status->ax) > 2.5 ||
             sensor_abs(status->ay) > 2.5 || sensor_abs(status->az) > 3.5){
        status->abnormal = true;
        strcpy(status->alert,"ICM20608 motion abnormal");
    }
}

static int clamp_percent(int value)
{
    if(value < 0)
        return 0;
    if(value > 100)
        return 100;
    return value;
}

static int light_to_percent(int lux)
{
    if(lux <= 0)
        return 0;
    if(lux <= 100)
        return lux * 35 / 100;
    if(lux <= 500)
        return 35 + (lux - 100) * 40 / 400;
    if(lux <= 1000)
        return 75 + (lux - 500) * 15 / 500;
    return clamp_percent(90 + (lux - 1000) * 10 / 4000);
}

static int proximity_to_percent(int proximity)
{
    if(proximity <= 0)
        return 0;
    return clamp_percent(proximity * 100 / 1023);
}

static void create_sensor_gauge(lv_obj_t *parent,
                                sensor_gauge_view_t *gauge,
                                const char *title,
                                uint32_t color)
{
    lv_obj_t *item = lv_obj_create(parent);

    lv_obj_set_size(item,SENSOR_GAUGE_ITEM_WIDTH,SENSOR_GAUGE_ITEM_HEIGHT);
    lv_obj_add_style(item,&com_style,0);
    make_sleep_obj_transparent(item);

    gauge->arc = lv_arc_create(item);
    lv_obj_set_size(gauge->arc,SENSOR_GAUGE_SIZE,SENSOR_GAUGE_SIZE);
    lv_arc_set_range(gauge->arc,0,100);
    lv_arc_set_value(gauge->arc,0);
    lv_arc_set_bg_angles(gauge->arc,135,45);
    lv_obj_set_style_arc_width(gauge->arc,10,LV_PART_MAIN);
    lv_obj_set_style_arc_width(gauge->arc,10,LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(gauge->arc,lv_color_hex(0x30343B),LV_PART_MAIN);
    lv_obj_set_style_arc_color(gauge->arc,lv_color_hex(color),LV_PART_INDICATOR);
    lv_obj_remove_style(gauge->arc,NULL,LV_PART_KNOB);
    lv_obj_clear_flag(gauge->arc,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(gauge->arc,LV_ALIGN_TOP_MID,0,0);

    gauge->value_label = lv_label_create(item);
    obj_font_set(gauge->value_label,FONT_TYPE_NUMBER,FONT_SIZE_TITLE_1);
    lv_label_set_text(gauge->value_label,"--");
    lv_obj_set_style_text_color(gauge->value_label,lv_color_hex(color),0);
    lv_obj_align_to(gauge->value_label,gauge->arc,LV_ALIGN_CENTER,0,-9);

    gauge->unit_label = lv_label_create(item);
    obj_font_set(gauge->unit_label,FONT_TYPE_LETTER,FONT_SIZE_TEXT_5);
    lv_label_set_text(gauge->unit_label,"");
    lv_obj_set_style_text_color(gauge->unit_label,lv_color_hex(color),0);
    lv_obj_align_to(gauge->unit_label,gauge->value_label,LV_ALIGN_OUT_BOTTOM_MID,0,4);

    gauge->title_label = lv_label_create(item);
    obj_font_set(gauge->title_label,FONT_TYPE_CN,FONT_SIZE_TEXT_3);
    lv_label_set_text(gauge->title_label,title);
    lv_obj_set_style_text_color(gauge->title_label,APP_COLOR_WHITE,0);
    lv_obj_align_to(gauge->title_label,gauge->arc,LV_ALIGN_OUT_BOTTOM_MID,0,4);
}

static void set_sensor_gauge(sensor_gauge_view_t *gauge,
                             int percent,
                             const char *value,
                             const char *unit,
                             uint32_t color)
{
    int clamped_percent;

    if(gauge == NULL || gauge->arc == NULL)
        return;

    clamped_percent = clamp_percent(percent);
    if(!gauge->cache_valid || gauge->cached_percent != clamped_percent){
        lv_arc_set_value(gauge->arc,clamped_percent);
        gauge->cached_percent = clamped_percent;
    }
    if(!gauge->cache_valid || gauge->cached_color != color){
        lv_obj_set_style_arc_color(gauge->arc,lv_color_hex(color),LV_PART_INDICATOR);
        lv_obj_set_style_text_color(gauge->value_label,lv_color_hex(color),0);
        lv_obj_set_style_text_color(gauge->unit_label,lv_color_hex(color),0);
        gauge->cached_color = color;
    }
    if(!gauge->cache_valid || strcmp(gauge->cached_value,value) != 0){
        lv_label_set_text(gauge->value_label,value);
        strncpy(gauge->cached_value,value,sizeof(gauge->cached_value) - 1);
        gauge->cached_value[sizeof(gauge->cached_value) - 1] = '\0';
    }
    if(!gauge->cache_valid || strcmp(gauge->cached_unit,unit) != 0){
        lv_label_set_text(gauge->unit_label,unit);
        strncpy(gauge->cached_unit,unit,sizeof(gauge->cached_unit) - 1);
        gauge->cached_unit[sizeof(gauge->cached_unit) - 1] = '\0';
    }
    gauge->cache_valid = true;
}

static bool set_label_text_if_changed(lv_obj_t *label,const char *text)
{
    if(label != NULL && text != NULL && strcmp(lv_label_get_text(label),text) != 0){
        lv_label_set_text(label,text);
        return true;
    }
    return false;
}

static bool get_system_time(void)
{
    time(&timep);
    memcpy(&time_temp,localtime(&timep),sizeof(struct tm));
    snprintf(time_str,sizeof(time_str),"%02d:%02d",time_temp.tm_hour,time_temp.tm_min);
    return time_temp.tm_hour == 0 && time_temp.tm_min == 0 && time_temp.tm_sec == 1;
}

static void create_time_item(lv_obj_t *parent)
{
    lv_obj_t *item = lv_obj_create(parent);
    lv_obj_t *clock = NULL;

    lv_obj_set_size(item,CLOCK_ITEM_WIDTH,CLOCK_ITEM_HEIGHT);
    lv_obj_add_style(item,&com_style,0);
    make_sleep_obj_transparent(item);

    if(page_type == TIME_TYPE_1){
        time_label = lv_label_create(item);
        obj_font_set(time_label,FONT_TYPE_LETTER,FONT_SIZE_MAX);
        lv_label_set_text(time_label,time_str);
        lv_obj_set_style_text_color(time_label,APP_COLOR_WHITE,0);
        lv_obj_align(time_label,LV_ALIGN_CENTER,0,-34);

        weather_label = lv_label_create(item);
        obj_font_set(weather_label,FONT_TYPE_CN,FONT_SIZE_TEXT_2);
        lv_label_set_text(weather_label,get_device_state()->weather_info);
        lv_obj_set_style_text_color(weather_label,lv_color_hex(0xC8CDD4),0);
        lv_obj_align_to(weather_label,time_label,LV_ALIGN_OUT_BOTTOM_MID,0,5);
    }else if(page_type == TIME_TYPE_2){
        time_label = lv_label_create(item);
        obj_font_set(time_label,FONT_TYPE_NUMBER,FONT_SIZE_MAX);
        lv_label_set_text(time_label,time_str);
        lv_obj_set_style_text_color(time_label,APP_COLOR_WHITE,0);
        lv_obj_align(time_label,LV_ALIGN_CENTER,0,-10);
    }else{
        if(clock_type == 0){
            active_clock = &lv_clock0;
            clock = init_clock0_obj(item,&lv_clock0);
            deinit_clock0_obj(&lv_clock0);
        }else if(clock_type == 1){
            active_clock = &lv_clock1;
            clock = init_clock1_obj(item,&lv_clock1);
            deinit_clock1_obj(&lv_clock1);
        }else{
            active_clock = &lv_clock2;
            clock = init_clock2_obj(item,&lv_clock2);
            deinit_clock2_obj(&lv_clock2);
        }
        lv_obj_align(clock,LV_ALIGN_TOP_MID,0,0);
    }

    node_status_label = lv_label_create(item);
    obj_font_set(node_status_label,FONT_TYPE_CN,FONT_SIZE_TEXT_5);
    lv_label_set_recolor(node_status_label,true);
    lv_label_set_text(node_status_label,"#7F8792 网关●#  #7F8792 STM32●#");
    lv_obj_align(node_status_label,LV_ALIGN_BOTTOM_MID,0,-2);
        if (page_type == TIME_TYPE_1 || page_type == TIME_TYPE_2) {
        lv_obj_set_style_translate_y(item, -20, 0);   // 数字时钟整体上移
    } else {
        lv_obj_set_style_translate_y(item, 0, 0);     // 模拟时钟保持原位
    }
}

static void refresh_sensor_status_view(void)
{
    sensor_status_t status;
    char value_text[32];
    char node_text[128];
    bool ap_abnormal;
    uint32_t light_color = 0xF2C14E;
    uint32_t temperature_color = 0xFF7866;
    uint32_t humidity_color = 0x57C7E3;
    uint32_t proximity_color = 0x67D391;

    if(light_gauge.arc == NULL || temperature_gauge.arc == NULL ||
       humidity_gauge.arc == NULL || proximity_gauge.arc == NULL ||
       node_status_label == NULL)
        return;

    read_sensor_status(&status);
    ap_abnormal = status.abnormal && !status.has_ap;

    if(status.offline){
        set_sensor_gauge(&light_gauge,0,"--","lux",0x777777);
        set_sensor_gauge(&temperature_gauge,0,"--","C",0x777777);
        set_sensor_gauge(&humidity_gauge,0,"--","%",0x777777);
        set_sensor_gauge(&proximity_gauge,0,"--","PS",0x777777);
        set_label_text_if_changed(node_status_label,"#F06464 网关●#  #7F8792 STM32●#");
    }else{
        if(ap_abnormal)
            light_color = 0xFF5555;
        snprintf(value_text,sizeof(value_text),"%d",status.lux);
        set_sensor_gauge(&light_gauge,light_to_percent(status.lux),value_text,"lux",light_color);

        if(status.stm32_online && status.dht11_ok){
            if(status.dht11_temperature < 10)
                temperature_color = 0x5DADE2;
            else if(status.dht11_temperature > 35)
                temperature_color = 0xFF5555;
            if(status.dht11_humidity < 30 || status.dht11_humidity > 70)
                humidity_color = 0xF2C14E;

            snprintf(value_text,sizeof(value_text),"%d",status.dht11_temperature);
            set_sensor_gauge(&temperature_gauge,
                             clamp_percent(status.dht11_temperature * 2),
                             value_text,"C",temperature_color);
            snprintf(value_text,sizeof(value_text),"%d",status.dht11_humidity);
            set_sensor_gauge(&humidity_gauge,
                             clamp_percent(status.dht11_humidity),
                             value_text,"%",humidity_color);
        }else{
            uint32_t dht_color = status.stm32_online ? 0xF2C14E : 0x777777;
            set_sensor_gauge(&temperature_gauge,0,"--","C",dht_color);
            set_sensor_gauge(&humidity_gauge,0,"--","%",dht_color);
        }

        if(status.proximity > 500)
            proximity_color = 0xF2C14E;
        snprintf(value_text,sizeof(value_text),"%d",status.proximity);
        set_sensor_gauge(&proximity_gauge,
                         proximity_to_percent(status.proximity),
                         value_text,"PS",proximity_color);

        snprintf(node_text,sizeof(node_text),
                 "#59D878 网关●#  #%s STM32●#",
                 status.stm32_online ? "59D878" : "F06464");
        set_label_text_if_changed(node_status_label,node_text);
    }

    if(!status.offline && status.abnormal && !sensor_warning_reported){
        set_sensor_warning_message(status.alert);
        init_page_warn(WARN_SENSOR_DATA_TRIGGER);
        sensor_warning_reported = true;
    }else if(status.offline || !status.abnormal){
        sensor_warning_reported = false;
    }
}

static void init_sensor_status_view(lv_obj_t *parent)
{
    lv_obj_t *gauge_row = lv_obj_create(parent);

    lv_obj_set_size(gauge_row,1100,220);
    lv_obj_add_style(gauge_row,&com_style,0);
    make_sleep_obj_transparent(gauge_row);
    lv_obj_set_flex_flow(gauge_row,LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(gauge_row,LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(gauge_row,50,0);
    lv_obj_center(gauge_row);

    create_sensor_gauge(gauge_row,&light_gauge,"光照强度",0xF2C14E);
    create_sensor_gauge(gauge_row,&temperature_gauge,"环境温度",0xFF7866);
    create_time_item(gauge_row);
    create_sensor_gauge(gauge_row,&humidity_gauge,"环境湿度",0x57C7E3);
    create_sensor_gauge(gauge_row,&proximity_gauge,"接近强度",0x67D391);

    refresh_sensor_status_view();
}

void refresh_page_sleep(void){
    if(page == NULL)
        return;

    if(page_type == TIME_TYPE_1 || page_type == TIME_TYPE_2){
        if(get_system_time())
            http_get_weather_async(WEATHER_KEY,get_device_state()->weather_city);
        set_label_text_if_changed(time_label,time_str);
        if(page_type == TIME_TYPE_1 && weather_label != NULL &&
           set_label_text_if_changed(weather_label,get_device_state()->weather_info))
            lv_obj_align_to(weather_label,time_label,LV_ALIGN_OUT_BOTTOM_MID,0,5);
    }else if(active_clock != NULL && active_clock->dial_img != NULL){
        lv_event_send(active_clock->dial_img,LV_EVENT_REFRESH,NULL);
    }
    refresh_sensor_status_view();
}

bool page_sleep_is_active(void)
{
    return page != NULL;
}

static void reset_sensor_status_view(void)
{
    memset(&light_gauge,0,sizeof(light_gauge));
    memset(&temperature_gauge,0,sizeof(temperature_gauge));
    memset(&humidity_gauge,0,sizeof(humidity_gauge));
    memset(&proximity_gauge,0,sizeof(proximity_gauge));
    time_label = NULL;
    weather_label = NULL;
    node_status_label = NULL;
    active_clock = NULL;
    sensor_warning_reported = false;
}

static void stop_sleep_btn_click_event_cb(lv_event_t * e){
    (void)e;
    lv_obj_t * act_scr = page;
    lv_disp_t * d = lv_obj_get_disp(act_scr);
    if (d->prev_scr == NULL && (d->scr_to_load == NULL || d->scr_to_load == act_scr))
    {
        lv_obj_del(act_scr);
        lv_style_reset(&com_style);
        page = NULL;
        reset_sensor_status_view();
    }
    deinit_clock0_obj(&lv_clock0);
    deinit_clock1_obj(&lv_clock1);
    deinit_clock2_obj(&lv_clock2);
}

static void init_full_screen_touch_layer(lv_obj_t *parent)
{
    lv_obj_t *touch_layer = lv_obj_create(parent);

    lv_obj_set_size(touch_layer,LV_PCT(100),LV_PCT(100));
    lv_obj_center(touch_layer);
    make_sleep_obj_transparent(touch_layer);
    lv_obj_add_flag(touch_layer,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(touch_layer,stop_sleep_btn_click_event_cb,
                        LV_EVENT_CLICKED,NULL);
    lv_obj_move_foreground(touch_layer);
}

void init_page_sleep()
{
    if(page != NULL)
        return;
    page_type = get_device_state()->time_type;
    clock_type = get_device_state()->clock_type;
    active_clock = NULL;
    get_system_time();
    com_style_init();
    lv_obj_t * cont = lv_obj_create(lv_layer_top());
    page = cont;
    lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
    lv_obj_center(cont);
    lv_obj_add_style(cont, &com_style, 0);
    lv_obj_clear_flag(cont,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(cont,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(cont,stop_sleep_btn_click_event_cb,LV_EVENT_CLICKED,NULL);

    init_sensor_status_view(cont);
    init_full_screen_touch_layer(cont);
}
