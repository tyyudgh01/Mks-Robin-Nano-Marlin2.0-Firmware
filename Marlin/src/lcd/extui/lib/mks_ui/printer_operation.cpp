/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2020 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */
#include "../../../../inc/MarlinConfigPre.h"

#if HAS_TFT_LVGL_UI

#include "draw_ui.h"
#include <lv_conf.h>

#include "../../../../gcode/gcode.h"
#include "../../../../module/temperature.h"
#include "../../../../module/planner.h"
#include "../../../../module/motion.h"
#include "../../../../sd/cardreader.h"
#include "../../../../inc/MarlinConfig.h"
#include "../../../../MarlinCore.h"
#include "../../../../gcode/queue.h"

#if ENABLED(POWER_LOSS_RECOVERY)
  #include "../../../../feature/powerloss.h"
#endif

extern uint32_t To_pre_view;
extern bool flash_preview_begin, default_preview_flg, gcode_preview_over;
void esp_port_begin(uint8_t interrupt);
void printer_state_polling() {
  char str_1[16];
  if (uiCfg.print_state == PAUSING) {
    lv_clear_cur_ui();
    lv_draw_dialog(DIALOG_TYPE_MACHINE_PAUSING_TIPS);
    #if ENABLED(SDSUPPORT)
      while(queue.ring_buffer.length) {
        queue.advance();
      }
      planner.synchronize();
      gcode.process_subcommands_now_P(PSTR("M25"));
      //save the positon
      uiCfg.current_x_position_bak = current_position.x;
      uiCfg.current_y_position_bak = current_position.y;
      uiCfg.current_z_position_bak = current_position.z;

      if (gCfgItems.pausePosZ != (float)-1) {
        gcode.process_subcommands_now_P(PSTR("G91"));
        sprintf_P(public_buf_l, PSTR("G1 Z%s"), dtostrf(gCfgItems.pausePosZ, 1, 1, str_1));
        gcode.process_subcommands_now(public_buf_l);
        gcode.process_subcommands_now_P(PSTR("G90"));
      }
      if (gCfgItems.pausePosX != (float)-1) {
        sprintf_P(public_buf_l, PSTR("G1 X%s"), dtostrf(gCfgItems.pausePosX, 1, 1, str_1));
        gcode.process_subcommands_now(public_buf_l);
      }
      if (gCfgItems.pausePosY != (float)-1) {
        sprintf_P(public_buf_l, PSTR("G1 Y%s"), dtostrf(gCfgItems.pausePosY, 1, 1, str_1));
        gcode.process_subcommands_now(public_buf_l);
      }
      uiCfg.print_state = PAUSED;
      uiCfg.current_e_position_bak = current_position.e;

      gCfgItems.pause_reprint = true;
      update_spi_flash();
      lv_clear_cur_ui();
      lv_draw_return_ui();
    #endif
  }

  if (uiCfg.print_state == PAUSED) {
  }

  if (uiCfg.print_state == RESUMING) {
    if (IS_SD_PAUSED()) {
      if (gCfgItems.pausePosX != (float)-1) {
        sprintf_P(public_buf_m, PSTR("G1 X%s"), dtostrf(uiCfg.current_x_position_bak, 1, 1, str_1));
        gcode.process_subcommands_now(public_buf_m);
      }
      if (gCfgItems.pausePosY != (float)-1) {
        sprintf_P(public_buf_m, PSTR("G1 Y%s"), dtostrf(uiCfg.current_y_position_bak, 1, 1, str_1));
        gcode.process_subcommands_now(public_buf_m);
      }
      if (gCfgItems.pausePosZ != (float)-1) {
        ZERO(public_buf_m);
        sprintf_P(public_buf_m, PSTR("G1 Z%s"), dtostrf(uiCfg.current_z_position_bak, 1, 1, str_1));
        gcode.process_subcommands_now(public_buf_m);
      }
      gcode.process_subcommands_now_P(M24_STR);
      uiCfg.print_state = WORKING;
      start_print_time();

      gCfgItems.pause_reprint = false;
      update_spi_flash();
    }
  }
  #if ENABLED(POWER_LOSS_RECOVERY)
    if (uiCfg.print_state == REPRINTED) {
      #if HAS_HOTEND
        HOTEND_LOOP() {
          const int16_t et = recovery.info.target_temperature[e];
          if (et) {
            #if HAS_MULTI_HOTEND
              sprintf_P(public_buf_m, PSTR("T%i"), e);
              gcode.process_subcommands_now(public_buf_m);
            #endif
            sprintf_P(public_buf_m, PSTR("M109 S%i"), et);
            gcode.process_subcommands_now(public_buf_m);
          }
        }
      #endif

      recovery.resume();

      #if 0
        // Move back to the saved XY
        char str_1[16], str_2[16];
        sprintf_P(public_buf_m, PSTR("G1 X%s Y%s F2000"),
          dtostrf(recovery.info.current_position.x, 1, 3, str_1),
          dtostrf(recovery.info.current_position.y, 1, 3, str_2)
        );
        gcode.process_subcommands_now(public_buf_m);

        if (gCfgItems.pause_reprint && gCfgItems.pausePosZ != -1.0f) {
          gcode.process_subcommands_now_P(PSTR("G91"));
          sprintf_P(public_buf_l, PSTR("G1 Z-%.1f"), gCfgItems.pausePosZ);
          gcode.process_subcommands_now(public_buf_l);
          gcode.process_subcommands_now_P(PSTR("G90"));
        }
      #endif
      uiCfg.print_state = WORKING;
      start_print_time();

      gCfgItems.pause_reprint = false;
      update_spi_flash();
    }
  #endif

  if (uiCfg.print_state == WORKING) {
    filament_check(); // filament_check();
  }
    
  TERN_(MKS_WIFI_MODULE, wifi_looping());

  #if ENABLED(AUTO_BED_LEVELING_BILINEAR)
    if (uiCfg.autoLeveling) {
      get_gcode_command(AUTO_LEVELING_COMMAND_ADDR, (uint8_t *)public_buf_m);
      public_buf_m[sizeof(public_buf_m) - 1] = 0;
      gcode.process_subcommands_now_P(PSTR(public_buf_m));
      lv_clear_cur_ui();
      #ifdef BLTOUCH
      bltouch_do_init(false);
      lv_draw_bltouch_settings();
      #endif
      #ifdef TOUCH_MI_PROBE
      lv_draw_touchmi_settings();
      #endif
      uiCfg.autoLeveling = false;
    }
  #endif
}

filament_check_t filament_c;

void filament_pin_setup() {
  #if PIN_EXISTS(MT_DET_1)
    SET_INPUT_PULLUP(MT_DET_1_PIN);
  #endif
  #if PIN_EXISTS(MT_DET_2)
    SET_INPUT_PULLUP(MT_DET_2_PIN);
  #endif
  #if PIN_EXISTS(MT_DET_3)
    SET_INPUT_PULLUP(MT_DET_3_PIN);
  #endif

  filament_c.status = F_STATUS_CHECK;
  filament_c.tick_delay = MT_TIME_DELAY;
  filament_c.tick_start = 0;
  filament_c.tick_end = 0;
}

void filament_check() {
  #if (PIN_EXISTS(MT_DET_1) || PIN_EXISTS(MT_DET_2) || PIN_EXISTS(MT_DET_3))
    const int FIL_DELAY = 20;
  #endif
  #if PIN_EXISTS(MT_DET_1)
    static int fil_det_count_1 = 0;
    if (!READ(MT_DET_1_PIN) && !MT_DET_PIN_INVERTING)
      fil_det_count_1++;
    else if (READ(MT_DET_1_PIN) && MT_DET_PIN_INVERTING)
      fil_det_count_1++;
    else if (fil_det_count_1 > 0)
      fil_det_count_1--;

    if (!READ(MT_DET_1_PIN) && !MT_DET_PIN_INVERTING)
      fil_det_count_1++;
    else if (READ(MT_DET_1_PIN) && MT_DET_PIN_INVERTING)
      fil_det_count_1++;
    else if (fil_det_count_1 > 0)
      fil_det_count_1--;
  #endif

  #if PIN_EXISTS(MT_DET_2)
    static int fil_det_count_2 = 0;
    if (!READ(MT_DET_2_PIN) && !MT_DET_PIN_INVERTING)
      fil_det_count_2++;
    else if (READ(MT_DET_2_PIN) && MT_DET_PIN_INVERTING)
      fil_det_count_2++;
    else if (fil_det_count_2 > 0)
      fil_det_count_2--;

    if (!READ(MT_DET_2_PIN) && !MT_DET_PIN_INVERTING)
      fil_det_count_2++;
    else if (READ(MT_DET_2_PIN) && MT_DET_PIN_INVERTING)
      fil_det_count_2++;
    else if (fil_det_count_2 > 0)
      fil_det_count_2--;
  #endif

  #if PIN_EXISTS(MT_DET_3)
    static int fil_det_count_3 = 0;
    if (!READ(MT_DET_3_PIN) && !MT_DET_PIN_INVERTING)
      fil_det_count_3++;
    else if (READ(MT_DET_3_PIN) && MT_DET_PIN_INVERTING)
      fil_det_count_3++;
    else if (fil_det_count_3 > 0)
      fil_det_count_3--;

    if (!READ(MT_DET_3_PIN) && !MT_DET_PIN_INVERTING)
      fil_det_count_3++;
    else if (READ(MT_DET_3_PIN) && MT_DET_PIN_INVERTING)
      fil_det_count_3++;
    else if (fil_det_count_3 > 0)
      fil_det_count_3--;
  #endif

  if (false
    #if PIN_EXISTS(MT_DET_1)
      || fil_det_count_1 >= FIL_DELAY
    #endif
    #if PIN_EXISTS(MT_DET_2)
      || fil_det_count_2 >= FIL_DELAY
    #endif
    #if PIN_EXISTS(MT_DET_3)
      || fil_det_count_3 >= FIL_DELAY
    #endif
  ) {
    lv_clear_cur_ui();
    TERN_(SDSUPPORT, card.pauseSDPrint());
    stop_print_time();
    uiCfg.print_state = PAUSING;

    if (gCfgItems.from_flash_pic)
      flash_preview_begin = true;
    else
      default_preview_flg = true;

    lv_draw_printing();
  }
}


bool get_filemant_pins(void) {

  #if PIN_EXISTS(MT_DET_1)
    if(READ(MT_DET_1_PIN) == MT_DET_PIN_INVERTING) {
      return false;
    }
  #endif

  #if PIN_EXISTS(MT_DET_2)
    if(READ(MT_DET_2_PIN) == MT_DET_PIN_INVERTING) {
       return false;
    }
  #endif

  #if PIN_EXISTS(MT_DET_3)
    if(READ(MT_DET_3_PIN) == MT_DET_PIN_INVERTING) {
       return false;
    }
  #endif

  return true;
}

void filament_check_2() {

  switch(filament_c.status) {

    case F_STATUS_CHECK:

      if(get_filemant_pins() == false) {
        filament_c.status = F_STATUS_WAIT;
        filament_c.tick_start = millis(); 
      }

    break;

    case F_STATUS_WAIT:
      filament_c.tick_end = millis(); 

      if(get_filemant_pins() == false) {

          if(filament_c.tick_end - filament_c.tick_start > filament_c.tick_delay) {  
              filament_c.status = F_STATUS_RUN;
          }
      }else {
        filament_c.status = F_STATUS_END;
      }
    break;

    case F_STATUS_RUN:
      lv_clear_cur_ui();
      TERN_(SDSUPPORT, card.pauseSDPrint());
      stop_print_time();
      uiCfg.print_state = PAUSING;

      if (gCfgItems.from_flash_pic)
        flash_preview_begin = true;
      else
        default_preview_flg = true;

      lv_draw_printing();
      filament_c.status = F_STATUS_WAIT_UP;
    break;

    case F_STATUS_WAIT_UP:
      if(get_filemant_pins() == true) {
        filament_c.status = F_STATUS_END;
      }
    break;

    case F_STATUS_END:
      filament_c.tick_start = 0;
      filament_c.tick_end  = 0;
      filament_c.status = F_STATUS_CHECK;
    break;
  }
}

void filament_set_status (filament_status_t status) {
  filament_c.status = status;
}

#endif // HAS_TFT_LVGL_UI
