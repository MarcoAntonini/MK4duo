/**
 * MK4duo 3D Printer Firmware
 *
 * Based on Marlin, Sprinter and grbl
 * Copyright (C) 2011 Camiel Gubbels / Erik van der Zalm
 * Copyright (C) 2013 - 2017 Alberto Cotronei @MagoKimbra
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

/**
 * eeprom.cpp
 *
 * Configuration and EEPROM storage
 *
 * IMPORTANT:  Whenever there are changes made to the variables stored in EEPROM
 * in the functions below, also increment the version number. This makes sure that
 * the default values are used whenever there is a change to the data, to prevent
 * wrong data being written to the variables.
 *
 * ALSO: Variables in the Store and Retrieve sections must be in the same order.
 *       If a feature is disabled, some data must still be written that, when read,
 *       either sets a Sane Default, or results in No Change to the existing value.
 *
 */

#include "../../base.h"

#define EEPROM_VERSION "MKV31"

/**
 * MKV431 EEPROM Layout:
 *
 *  Version (char x6)
 *  EEPROM Checksum (uint16_t)
 *
 *  M92   XYZ E0 ...      planner.axis_steps_per_mm X,Y,Z,E0 ... (float x9)
 *  M203  XYZ E0 ...      planner.max_feedrate_mm_s X,Y,Z,E0 ... (float x9)
 *  M201  XYZ E0 ...      planner.max_acceleration_mm_per_s2 X,Y,Z,E0 ... (uint32_t x9)
 *  M204  P               planner.acceleration (float)
 *  M204  R   E0 ...      planner.retract_acceleration (float x6)
 *  M204  T               planner.travel_acceleration (float)
 *  M205  S               planner.min_feedrate_mm_s (float)
 *  M205  T               planner.min_travel_feedrate_mm_s (float)
 *  M205  B               planner.min_segment_time (ulong)
 *  M205  X               planner.max_jerk[X_AXIS] (float)
 *  M205  Y               planner.max_jerk[Y_AXIS] (float)
 *  M205  Z               planner.max_jerk[Z_AXIS] (float)
 *  M205  E   E0 ...      planner.max_jerk[E_AXIS * EXTRDURES] (float x6)
 *  M206  XYZ             home_offset (float x3)
 *  M218  T   XY          hotend_offset (float x6)
 *
 * Global Leveling:
 *                        z_fade_height           (float)
 *
 * MESH_BED_LEVELING:
 *  M420  S               from mbl.status (bool)
 *                        mbl.z_offset (float)
 *                        GRID_MAX_POINTS_X (uint8 as set in firmware)
 *                        GRID_MAX_POINTS_Y (uint8 as set in firmware)
 *  G29   S3  XYZ         z_values[][] (float x9, by default, up to float x 81) +288
 *
 * ABL_PLANAR:
 *                        planner.bed_level_matrix        (matrix_3x3 = float x9)
 *
 * AUTO_BED_LEVELING_BILINEAR:
 *                        GRID_MAX_POINTS_X               (uint8_t)
 *                        GRID_MAX_POINTS_Y               (uint8_t)
 *                        bilinear_grid_spacing           (int x2)   from G29: (B-F)/X, (R-L)/Y
 *  G29   L F             bilinear_start                  (int x2)
 *                        z_values[][]                    (float x9, up to float x256)
 *
 * HAS_BED_PROBE:
 *  M666  P               zprobe_zoffset (float)
 *
 * HOTENDS AD595:
 *  M595  H OS            Hotend AD595 Offset & Gain
 *
 * DELTA:
 *  M666  XYZ             deltaParams.endstop_adj (float x3)
 *  M666  R               deltaParams.delta_radius (float)
 *  M666  D               deltaParams.diagonal_rod (float)
 *  M666  S               deltaParams.segments_per_second (float)
 *  M666  H               deltaParams.delta_height (float)
 *  M666  ABC             deltaParams.tower_radius_adj (float x3)
 *  M666  IJK             deltaParams.tower_pos_adj (float x3)
 *  M666  UVW             deltaParams.diagonal_rod_adj (float x3)
 *  M666  O               deltaParams.print_radius (float)
 *
 * Z_TWO_ENDSTOPS:
 *  M666  Z               z2_endstop_adj (float)
 *
 * ULTIPANEL:
 *  M145  S0  H           lcd_preheat_hotend_temp (int x3)
 *  M145  S0  B           lcd_preheat_bed_temp (int x3)
 *  M145  S0  F           lcd_preheat_fan_speed (int x3)
 *
 * PIDTEMP:
 *  M301  E0  PIDC        Kp[0], Ki[0], Kd[0], Kc[0] (float x4)
 *  M301  E1  PIDC        Kp[1], Ki[1], Kd[1], Kc[1] (float x4)
 *  M301  E2  PIDC        Kp[2], Ki[2], Kd[2], Kc[2] (float x4)
 *  M301  E3  PIDC        Kp[3], Ki[3], Kd[3], Kc[3] (float x4)
 *  M301  L               lpq_len
 *
 * PIDTEMPBED:
 *  M304      PID         bedKp, bedKi, bedKd (float x3)
 * PIDTEMPCHAMBER
 *  M305      PID         chamberKp, chamberKi, chamberKd (float x3)
 * PIDTEMPCOOLER
 *  M306      PID         coolerKp, coolerKi, coolerKd (float x3)
 *
 * DOGLCD:
 *  M250  C               lcd_contrast (uint16_t)
 *
 * FWRETRACT:
 *  M209  S               autoretract_enabled (bool)
 *  M207  S               retract_length (float)
 *  M207  W               retract_length_swap (float)
 *  M207  F               retract_feedrate (float)
 *  M207  Z               retract_zlift (float)
 *  M208  S               retract_recover_length (float)
 *  M208  W               retract_recover_length_swap (float)
 *  M208  F               retract_recover_feedrate (float)
 *
 * Volumetric Extrusion:
 *  M200  D               volumetric_enabled (bool)
 *  M200  T D             filament_size (float x6)
 *
 *  M???  S               IDLE_OOZING_enabled
 *
 * ALLIGATOR:
 *  M906  XYZ T0-4 E      Motor current (float x7)
 *
 * HAVE_TMC2130:
 *  M906  X               stepperX current  (uint16_t)
 *  M906  Y               stepperY current  (uint16_t)
 *  M906  Z               stepperZ current  (uint16_t)
 *  M906  X2              stepperX2 current (uint16_t)
 *  M906  Y2              stepperY2 current (uint16_t)
 *  M906  Z2              stepperZ2 current (uint16_t)
 *  M906  E0              stepperE0 current (uint16_t)
 *  M906  E1              stepperE1 current (uint16_t)
 *  M906  E2              stepperE2 current (uint16_t)
 *  M906  E3              stepperE3 current (uint16_t)
 *  M906  E4              stepperE4 current (uint16_t)
 *  M906  E5              stepperE5 current (uint16_t)
 *
 * LIN_ADVANCE:
 *  M900  K               extruder_advance_k (float)
 *  M900  WHD             advance_ed_ratio   (float)
 *
 */

EEPROM eeprom;

/**
 * Post-process after Retrieve or Reset
 */
void EEPROM::Postprocess() {
  // steps per s2 needs to be updated to agree with units per s2
  planner.reset_acceleration_rates();

  // Make sure delta kinematics are updated before refreshing the
  // planner position so the stepper counts will be set correctly.
  #if MECH(DELTA)
    deltaParams.Recalc();
  #endif

  // Refresh steps_to_mm with the reciprocal of axis_steps_per_mm
  // and init stepper.count[], planner.position[] with current_position
  planner.refresh_positioning();

  #if ENABLED(PIDTEMP)
    thermalManager.updatePID();
  #endif

  calculate_volumetric_multipliers();

  #if ENABLED(WORKSPACE_OFFSETS) || ENABLED(DUAL_X_CARRIAGE)
    // Software endstops depend on home_offset
    LOOP_XYZ(i) update_software_endstops((AxisEnum)i);
  #endif

  #if HAS_LEVELING && ENABLED(ENABLE_LEVELING_FADE_HEIGHT)
    set_z_fade_height(planner.z_fade_height);
  #endif

  #if ENABLED(AUTO_BED_LEVELING_BILINEAR)
    refresh_bed_level();
  #endif
}

#if ENABLED(EEPROM_SETTINGS)

  const char version[6] = EEPROM_VERSION;

  uint16_t EEPROM::eeprom_checksum;

  bool  EEPROM::eeprom_write_error,
        EEPROM::eeprom_read_error;

  #if HAS(EEPROM_SD)

    void EEPROM::write_data(int &pos, const uint8_t* value, uint16_t size) {
      if (eeprom_write_error) return;

      while(size--) {
        const uint8_t v = *value;
        if (!card.write_data(v)) {
          SERIAL_LM(ECHO, MSG_ERR_EEPROM_WRITE);
          eeprom_write_error = true;
          return;
        }
        eeprom_checksum += v;
        pos++;
        value++;
      };
    }

    void EEPROM::read_data(int &pos, uint8_t* value, uint16_t size) {
      if (eeprom_read_error) return;

      do {
        uint8_t c = card.read_data();
        *value = c;
        eeprom_checksum += c;
        pos++;
        value++;
      } while (--size);
    }

  #else

    void EEPROM::write_data(int &pos, const uint8_t* value, uint16_t size) {
      if (eeprom_write_error) return;

      while(size--) {
        uint8_t * const p = (uint8_t * const)pos;
        const uint8_t v = *value;
        // EEPROM has only ~100,000 write cycles,
        // so only write bytes that have changed!
        if (v != eeprom_read_byte(p)) {
          eeprom_write_byte(p, v);
          if (eeprom_read_byte(p) != v) {
            SERIAL_LM(ECHO, MSG_ERR_EEPROM_WRITE);
            eeprom_write_error = true;
            return;
          }
        }
        eeprom_checksum += v;
        pos++;
        value++;
      };
    }

    void EEPROM::read_data(int &pos, uint8_t* value, uint16_t size) {
      do {
        uint8_t c = eeprom_read_byte((unsigned char*)pos);
        if (!eeprom_read_error) *value = c;
        eeprom_checksum += c;
        pos++;
        value++;
      } while (--size);
    }

  #endif

  #define EEPROM_START()    int eeprom_index = EEPROM_OFFSET
  #define EEPROM_SKIP(VAR)  eeprom_index += sizeof(VAR)
  #define EEPROM_WRITE(VAR) write_data(eeprom_index, (uint8_t*)&VAR, sizeof(VAR))
  #define EEPROM_READ(VAR)  read_data(eeprom_index, (uint8_t*)&VAR, sizeof(VAR))

  /**
   * M500 - Store Configuration
   */
  bool EEPROM::Store_Settings() {
    float dummy = 0.0f;
    char ver[6] = "00000";

    EEPROM_START();

    eeprom_write_error = false;

    #if HAS(EEPROM_SD)
      // EEPROM on SDCARD
      if (!IS_SD_INSERTED || card.isFileOpen() || card.sdprinting) {
        SERIAL_LM(ER, MSG_NO_CARD);
        return false;
      }
      set_sd_dot();
      card.setroot(true);
      card.startWrite((char *)"EEPROM.bin", false);
      EEPROM_WRITE(version);
      eeprom_checksum = 0; // clear before first "real data"
    #else
      // EEPROM on SPI or IC2
      EEPROM_WRITE(ver);     // invalidate data first
      EEPROM_SKIP(eeprom_checksum); // Skip the checksum slot
      eeprom_checksum = 0; // clear before first "real data"
    #endif

    EEPROM_WRITE(planner.axis_steps_per_mm);
    EEPROM_WRITE(planner.max_feedrate_mm_s);
    EEPROM_WRITE(planner.max_acceleration_mm_per_s2);
    EEPROM_WRITE(planner.acceleration);
    EEPROM_WRITE(planner.retract_acceleration);
    EEPROM_WRITE(planner.travel_acceleration);
    EEPROM_WRITE(planner.min_feedrate_mm_s);
    EEPROM_WRITE(planner.min_travel_feedrate_mm_s);
    EEPROM_WRITE(planner.min_segment_time);
    EEPROM_WRITE(planner.max_jerk);
    #if DISABLED(WORKSPACE_OFFSETS)
      const float home_offset[XYZ] = { 0 };
    #endif
    EEPROM_WRITE(home_offset);
    EEPROM_WRITE(hotend_offset);

    //
    // General Leveling
    //
    #if ENABLED(ENABLE_LEVELING_FADE_HEIGHT)
      EEPROM_WRITE(planner.z_fade_height);
    #endif

    //
    // Mesh Bed Leveling
    //
    #if ENABLED(MESH_BED_LEVELING)
      // Compile time test that sizeof(mbl.z_values) is as expected
      static_assert(
        sizeof(mbl.z_values) == GRID_MAX_POINTS * sizeof(mbl.z_values[0][0]),
        "MBL Z array is the wrong size."
      );
      const bool leveling_is_on = TEST(mbl.status, MBL_STATUS_HAS_MESH_BIT);
      const uint8_t mesh_num_x = GRID_MAX_POINTS_X, mesh_num_y = GRID_MAX_POINTS_Y;
      EEPROM_WRITE(leveling_is_on);
      EEPROM_WRITE(mbl.z_offset);
      EEPROM_WRITE(mesh_num_x);
      EEPROM_WRITE(mesh_num_y);
      EEPROM_WRITE(mbl.z_values);
    #endif // MESH_BED_LEVELING

    //
    // Planar Bed Leveling matrix
    //
    #if ABL_PLANAR
      EEPROM_WRITE(planner.bed_level_matrix);
    #endif

    //
    // Bilinear Auto Bed Leveling
    //
    #if ENABLED(AUTO_BED_LEVELING_BILINEAR)
      static_assert(
        sizeof(z_values) == GRID_MAX_POINTS * sizeof(z_values[0][0]),
        "Bilinear Z array is the wrong size."
      );
      const uint8_t grid_max_x = GRID_MAX_POINTS_X, grid_max_y = GRID_MAX_POINTS_Y;
      EEPROM_WRITE(grid_max_x);             // 1 byte
      EEPROM_WRITE(grid_max_y);             // 1 byte
      EEPROM_WRITE(bilinear_grid_spacing);  // 2 ints
      EEPROM_WRITE(bilinear_start);         // 2 ints
      EEPROM_WRITE(z_values);               // 9-256 floats
    #endif // AUTO_BED_LEVELING_BILINEAR

    #if HASNT(BED_PROBE)
      float zprobe_zoffset = 0;
    #endif
    EEPROM_WRITE(zprobe_zoffset);

    #if HEATER_USES_AD595
      EEPROM_WRITE(ad595_offset);
      EEPROM_WRITE(ad595_gain);
    #endif

    #if MECH(DELTA)
      EEPROM_WRITE(deltaParams.endstop_adj);
      EEPROM_WRITE(deltaParams.delta_radius);
      EEPROM_WRITE(deltaParams.diagonal_rod);
      EEPROM_WRITE(deltaParams.segments_per_second);
      EEPROM_WRITE(deltaParams.delta_height);
      EEPROM_WRITE(deltaParams.tower_radius_adj);
      EEPROM_WRITE(deltaParams.tower_pos_adj);
      EEPROM_WRITE(deltaParams.diagonal_rod_adj);
      EEPROM_WRITE(deltaParams.print_radius);
    #elif ENABLED(Z_TWO_ENDSTOPS)
      EEPROM_WRITE(z2_endstop_adj);
    #endif

    #if DISABLED(ULTIPANEL)
      const int lcd_preheat_hotend_temp[3] = { PREHEAT_1_TEMP_HOTEND, PREHEAT_2_TEMP_HOTEND, PREHEAT_3_TEMP_HOTEND },
                lcd_preheat_bed_temp[3] = { PREHEAT_1_TEMP_BED, PREHEAT_2_TEMP_BED, PREHEAT_3_TEMP_BED },
                lcd_preheat_fan_speed[3] = { PREHEAT_1_FAN_SPEED, PREHEAT_2_FAN_SPEED, PREHEAT_3_FAN_SPEED };
    #endif

    EEPROM_WRITE(lcd_preheat_hotend_temp);
    EEPROM_WRITE(lcd_preheat_bed_temp);
    EEPROM_WRITE(lcd_preheat_fan_speed);

    #if ENABLED(PIDTEMP)
      for (uint8_t h = 0; h < HOTENDS; h++) {
        EEPROM_WRITE(PID_PARAM(Kp, h));
        EEPROM_WRITE(PID_PARAM(Ki, h));
        EEPROM_WRITE(PID_PARAM(Kd, h));
        EEPROM_WRITE(PID_PARAM(Kc, h));
      }
    #endif

    #if DISABLED(PID_ADD_EXTRUSION_RATE)
      int lpq_len = 20;
    #endif
    EEPROM_WRITE(lpq_len);
    
    #if ENABLED(PIDTEMPBED)
      EEPROM_WRITE(thermalManager.bedKp);
      EEPROM_WRITE(thermalManager.bedKi);
      EEPROM_WRITE(thermalManager.bedKd);
    #endif

    #if ENABLED(PIDTEMPCHAMBER)
      EEPROM_WRITE(thermalManager.chamberKp);
      EEPROM_WRITE(thermalManager.chamberKi);
      EEPROM_WRITE(thermalManager.chamberKd);
    #endif

    #if ENABLED(PIDTEMPCOOLER)
      EEPROM_WRITE(thermalManager.coolerKp);
      EEPROM_WRITE(thermalManager.coolerKi);
      EEPROM_WRITE(thermalManager.coolerKd);
    #endif

    #if HASNT(LCD_CONTRAST)
      const uint16_t lcd_contrast = 32;
    #endif
    EEPROM_WRITE(lcd_contrast);

    #if ENABLED(FWRETRACT)
      EEPROM_WRITE(autoretract_enabled);
      EEPROM_WRITE(retract_length);
      #if EXTRUDERS > 1
        EEPROM_WRITE(retract_length_swap);
      #else
        EEPROM_WRITE(dummy);
      #endif
      EEPROM_WRITE(retract_feedrate);
      EEPROM_WRITE(retract_zlift);
      EEPROM_WRITE(retract_recover_length);
      #if EXTRUDERS > 1
        EEPROM_WRITE(retract_recover_length_swap);
      #else
        EEPROM_WRITE(dummy);
      #endif
      EEPROM_WRITE(retract_recover_feedrate);
    #endif // FWRETRACT

    EEPROM_WRITE(volumetric_enabled);

    // Save filament sizes
    for (uint8_t e = 0; e < EXTRUDERS; e++)
      EEPROM_WRITE(filament_size[e]);

    #if ENABLED(IDLE_OOZING_PREVENT)
      EEPROM_WRITE(IDLE_OOZING_enabled);
    #endif

    #if MB(ALLIGATOR) || MB(ALLIGATOR_V3)
      EEPROM_WRITE(motor_current);
    #endif

    // Save TCM2130 Configuration, and placeholder values
    #if ENABLED(HAVE_TMC2130)
      uint16_t val;
      #if ENABLED(X_IS_TMC2130)
        val = stepperX.getCurrent();
      #else
        val = 0;
      #endif
      EEPROM_WRITE(val);
      #if ENABLED(Y_IS_TMC2130)
        val = stepperY.getCurrent();
      #else
        val = 0;
      #endif
      EEPROM_WRITE(val);
      #if ENABLED(Z_IS_TMC2130)
        val = stepperZ.getCurrent();
      #else
        val = 0;
      #endif
      EEPROM_WRITE(val);
      #if ENABLED(X2_IS_TMC2130)
        val = stepperX2.getCurrent();
      #else
        val = 0;
      #endif
      EEPROM_WRITE(val);
      #if ENABLED(Y2_IS_TMC2130)
        val = stepperY2.getCurrent();
      #else
        val = 0;
      #endif
      EEPROM_WRITE(val);
      #if ENABLED(Z2_IS_TMC2130)
        val = stepperZ2.getCurrent();
      #else
        val = 0;
      #endif
      EEPROM_WRITE(val);
      #if ENABLED(E0_IS_TMC2130)
        val = stepperE0.getCurrent();
      #else
        val = 0;
      #endif
      EEPROM_WRITE(val);
      #if ENABLED(E1_IS_TMC2130)
        val = stepperE1.getCurrent();
      #else
        val = 0;
      #endif
      EEPROM_WRITE(val);
      #if ENABLED(E2_IS_TMC2130)
        val = stepperE2.getCurrent();
      #else
        val = 0;
      #endif
      EEPROM_WRITE(val);
      #if ENABLED(E3_IS_TMC2130)
        val = stepperE3.getCurrent();
      #else
        val = 0;
      #endif
      EEPROM_WRITE(val);
      #if ENABLED(E4_IS_TMC2130)
        val = stepperE4.getCurrent();
      #else
        val = 0;
      #endif
      EEPROM_WRITE(val);
      #if ENABLED(E5_IS_TMC2130)
        val = stepperE5.getCurrent();
      #else
        val = 0;
      #endif
      EEPROM_WRITE(val);
    #endif

    //
    // Linear Advance
    //
    #if ENABLED(LIN_ADVANCE)
      EEPROM_WRITE(planner.extruder_advance_k);
      EEPROM_WRITE(planner.advance_ed_ratio);
    #endif

    if (!eeprom_write_error) {

      uint16_t  final_checksum = eeprom_checksum,
                eeprom_size = eeprom_index;

      eeprom_index = EEPROM_OFFSET;
      EEPROM_WRITE(version);
      EEPROM_WRITE(final_checksum);

      // Report storage size
      SERIAL_SMV(ECHO, "Settings Stored (", eeprom_size - (EEPROM_OFFSET));
      SERIAL_EM(" bytes)");
    }

    #if HAS(EEPROM_SD)
      card.finishWrite();
      unset_sd_dot();
    #endif

    return !eeprom_write_error;
  }

  /**
   * M501 - Retrieve Configuration
   */
  bool EEPROM::Load_Settings() {

    EEPROM_START();
    eeprom_read_error = false; // If set EEPROM_READ won't write into RAM

    char stored_ver[6];
    uint16_t stored_checksum;

    #if HAS(EEPROM_SD)
      if (IS_SD_INSERTED || !card.isFileOpen() || !card.sdprinting || card.cardOK) {
        set_sd_dot();
        card.setroot(true);
        card.selectFile((char *)"EEPROM.bin", true);
        EEPROM_READ(stored_ver);
      }
      else {
        SERIAL_LM(ER, MSG_NO_CARD);
        return false;
      }
    #else
      EEPROM_READ(stored_ver);
      EEPROM_READ(stored_checksum);
    #endif

    if (strncmp(version, stored_ver, 5) != 0) {
      if (stored_ver[0] != 'M') {
        stored_ver[0] = '?';
        stored_ver[1] = '?';
        stored_ver[2] = '\0';
      }
      SERIAL_SM(ECHO, "EEPROM version mismatch ");
      SERIAL_MT("(EEPROM=", stored_ver);
      SERIAL_EM(" MK4duo=" EEPROM_VERSION ")");
      Factory_Settings();
    }
    else {
      float dummy = 0;

      eeprom_checksum = 0; // clear before reading first "real data"

      // version number match
      EEPROM_READ(planner.axis_steps_per_mm);
      EEPROM_READ(planner.max_feedrate_mm_s);
      EEPROM_READ(planner.max_acceleration_mm_per_s2);

      EEPROM_READ(planner.acceleration);
      EEPROM_READ(planner.retract_acceleration);
      EEPROM_READ(planner.travel_acceleration);
      EEPROM_READ(planner.min_feedrate_mm_s);
      EEPROM_READ(planner.min_travel_feedrate_mm_s);
      EEPROM_READ(planner.min_segment_time);
      EEPROM_READ(planner.max_jerk);
      #if DISABLED(WORKSPACE_OFFSETS)
        float home_offset[XYZ];
      #endif
      EEPROM_READ(home_offset);
      EEPROM_READ(hotend_offset);

      //
      // General Leveling
      //
      #if ENABLED(ENABLE_LEVELING_FADE_HEIGHT)
        EEPROM_READ(planner.z_fade_height);
      #endif

      //
      // Mesh (Manual) Bed Leveling
      //
      #if ENABLED(MESH_BED_LEVELING)
        bool leveling_is_on;
        uint8_t mesh_num_x = 0, mesh_num_y = 0;
        EEPROM_READ(leveling_is_on);
        EEPROM_READ(dummy);
        EEPROM_READ(mesh_num_x);
        EEPROM_READ(mesh_num_y);
        mbl.status = leveling_is_on ? _BV(MBL_STATUS_HAS_MESH_BIT) : 0;
        mbl.z_offset = dummy;
        if (mesh_num_x == GRID_MAX_POINTS_X && mesh_num_y == GRID_MAX_POINTS_Y) {
          // EEPROM data fits the current mesh
          EEPROM_READ(mbl.z_values);
        }
        else {
          // EEPROM data is stale
          mbl.reset();
          for (uint8_t q = 0; q < mesh_num_x * mesh_num_y; q++) EEPROM_READ(dummy);
        }
      #endif // MESH_BED_LEVELING

      //
      // Planar Bed Leveling matrix
      //
      #if ABL_PLANAR
        EEPROM_READ(planner.bed_level_matrix);
      #endif

      //
      // Bilinear Auto Bed Leveling
      //
      #if ENABLED(AUTO_BED_LEVELING_BILINEAR)
        uint8_t grid_max_x, grid_max_y;
        EEPROM_READ(grid_max_x);              // 1 byte
        EEPROM_READ(grid_max_y);              // 1 byte
        if (grid_max_x == GRID_MAX_POINTS_X && grid_max_y == GRID_MAX_POINTS_Y) {
          set_bed_leveling_enabled(false);
          EEPROM_READ(bilinear_grid_spacing); // 2 ints
          EEPROM_READ(bilinear_start);        // 2 ints
          EEPROM_READ(z_values);              // 9 to 256 floats
        }
        else { // EEPROM data is stale
          // Skip past disabled (or stale) Bilinear Grid data
          int bgs[2], bs[2];
          EEPROM_READ(bgs);
          EEPROM_READ(bs);
          for (uint16_t q = grid_max_x * grid_max_y; q--;) EEPROM_READ(dummy);
        }
      #endif // AUTO_BED_LEVELING_BILINEAR

      #if HASNT(BED_PROBE)
        float zprobe_zoffset = 0;
      #endif
      EEPROM_READ(zprobe_zoffset);

      #if HEATER_USES_AD595
        EEPROM_READ(ad595_offset);
        EEPROM_READ(ad595_gain);
        for (int8_t h = 0; h < HOTENDS; h++)
          if (ad595_gain[h] == 0) ad595_gain[h] == TEMP_SENSOR_AD595_GAIN;
      #endif

      #if MECH(DELTA)
        EEPROM_READ(deltaParams.endstop_adj);
        EEPROM_READ(deltaParams.delta_radius);
        EEPROM_READ(deltaParams.diagonal_rod);
        EEPROM_READ(deltaParams.segments_per_second);
        EEPROM_READ(deltaParams.delta_height);
        EEPROM_READ(deltaParams.tower_radius_adj);
        EEPROM_READ(deltaParams.tower_pos_adj);
        EEPROM_READ(deltaParams.diagonal_rod_adj);
        EEPROM_READ(deltaParams.print_radius);
      #elif ENABLED(Z_TWO_ENDSTOPS)
        EEPROM_READ(z2_endstop_adj);
      #endif

      #if DISABLED(ULTIPANEL)
        int lcd_preheat_hotend_temp[3], lcd_preheat_bed_temp[3], lcd_preheat_fan_speed[3];
      #endif

      EEPROM_READ(lcd_preheat_hotend_temp);
      EEPROM_READ(lcd_preheat_bed_temp);
      EEPROM_READ(lcd_preheat_fan_speed);

      #if ENABLED(PIDTEMP)
        for (int8_t h = 0; h < HOTENDS; h++) {
          EEPROM_READ(PID_PARAM(Kp, h));
          EEPROM_READ(PID_PARAM(Ki, h));
          EEPROM_READ(PID_PARAM(Kd, h));
          EEPROM_READ(PID_PARAM(Kc, h));
        }
      #endif // PIDTEMP

      #if DISABLED(PID_ADD_EXTRUSION_RATE)
        int lpq_len;
      #endif
      EEPROM_READ(lpq_len);

      #if ENABLED(PIDTEMPBED)
        EEPROM_READ(thermalManager.bedKp);
        EEPROM_READ(thermalManager.bedKi);
        EEPROM_READ(thermalManager.bedKd);
      #endif

      #if ENABLED(PIDTEMPCHAMBER)
        EEPROM_READ(thermalManager.chamberKp);
        EEPROM_READ(thermalManager.chamberKi);
        EEPROM_READ(thermalManager.chamberKd);
      #endif

      #if ENABLED(PIDTEMPCOOLER)
        EEPROM_READ(thermalManager.coolerKp);
        EEPROM_READ(thermalManager.coolerKi);
        EEPROM_READ(thermalManager.coolerKd);
      #endif

      #if HASNT(LCD_CONTRAST)
        uint16_t lcd_contrast;
      #endif
      EEPROM_READ(lcd_contrast);

      #if ENABLED(FWRETRACT)
        EEPROM_READ(autoretract_enabled);
        EEPROM_READ(retract_length);
        #if EXTRUDERS > 1
          EEPROM_READ(retract_length_swap);
        #else
          EEPROM_READ(dummy);
        #endif
        EEPROM_READ(retract_feedrate);
        EEPROM_READ(retract_zlift);
        EEPROM_READ(retract_recover_length);
        #if EXTRUDERS > 1
          EEPROM_READ(retract_recover_length_swap);
        #else
          EEPROM_READ(dummy);
        #endif
        EEPROM_READ(retract_recover_feedrate);
      #endif // FWRETRACT

      EEPROM_READ(volumetric_enabled);

      for (int8_t e = 0; e < EXTRUDERS; e++)
        EEPROM_READ(filament_size[e]);

      #if ENABLED(IDLE_OOZING_PREVENT)
        EEPROM_READ(IDLE_OOZING_enabled);
      #endif

      #if MB(ALLIGATOR) || MB(ALLIGATOR_V3)
        EEPROM_READ(motor_current);
      #endif

      #if ENABLED(HAVE_TMC2130)
        uint16_t val;
        EEPROM_READ(val);
        #if ENABLED(X_IS_TMC2130)
          stepperX.setCurrent(val, R_SENSE, HOLD_MULTIPLIER);
        #endif
        EEPROM_READ(val);
        #if ENABLED(Y_IS_TMC2130)
          stepperY.setCurrent(val, R_SENSE, HOLD_MULTIPLIER);
        #endif
        EEPROM_READ(val);
        #if ENABLED(Z_IS_TMC2130)
          stepperZ.setCurrent(val, R_SENSE, HOLD_MULTIPLIER);
        #endif
        EEPROM_READ(val);
        #if ENABLED(X2_IS_TMC2130)
          stepperX2.setCurrent(val, R_SENSE, HOLD_MULTIPLIER);
        #endif
        EEPROM_READ(val);
        #if ENABLED(Y2_IS_TMC2130)
          stepperY2.setCurrent(val, R_SENSE, HOLD_MULTIPLIER);
        #endif
        EEPROM_READ(val);
        #if ENABLED(Z2_IS_TMC2130)
          stepperZ2.setCurrent(val, R_SENSE, HOLD_MULTIPLIER);
        #endif
        EEPROM_READ(val);
        #if ENABLED(E0_IS_TMC2130)
          stepperE0.setCurrent(val, R_SENSE, HOLD_MULTIPLIER);
        #endif
        EEPROM_READ(val);
        #if ENABLED(E1_IS_TMC2130)
          stepperE1.setCurrent(val, R_SENSE, HOLD_MULTIPLIER);
        #endif
        EEPROM_READ(val);
        #if ENABLED(E2_IS_TMC2130)
          stepperE2.setCurrent(val, R_SENSE, HOLD_MULTIPLIER);
        #endif
        EEPROM_READ(val);
        #if ENABLED(E3_IS_TMC2130)
          stepperE3.setCurrent(val, R_SENSE, HOLD_MULTIPLIER);
        #endif
        EEPROM_READ(val);
        #if ENABLED(E4_IS_TMC2130)
          stepperE4.setCurrent(val, R_SENSE, HOLD_MULTIPLIER);
        #endif
        EEPROM_READ(val);
        #if ENABLED(E5_IS_TMC2130)
          stepperE5.setCurrent(val, R_SENSE, HOLD_MULTIPLIER);
        #endif
      #endif

      //
      // Linear Advance
      //
      #if ENABLED(LIN_ADVANCE)
        EEPROM_READ(planner.extruder_advance_k);
        EEPROM_READ(planner.advance_ed_ratio);
      #endif

      #if HAS(EEPROM_SD)

        card.closeFile();
        unset_sd_dot();
        if (eeprom_read_error)
          Factory_Settings();
        else {
          Postprocess();
          SERIAL_V(version);
          SERIAL_MV(" stored settings retrieved (", eeprom_index - (EEPROM_OFFSET));
          SERIAL_EM(" bytes)");
        }

      #else

        if (eeprom_checksum == stored_checksum) {
          if (eeprom_read_error)
            Factory_Settings();
          else {
            Postprocess();
            SERIAL_V(version);
            SERIAL_MV(" stored settings retrieved (", eeprom_index - (EEPROM_OFFSET));
            SERIAL_EM(" bytes)");
          }
        }
        else {
          SERIAL_LM(ER, "EEPROM checksum mismatch");
          Factory_Settings();
        }

      #endif
    }

    #if ENABLED(EEPROM_CHITCHAT)
      Print_Settings();
    #endif

    return !eeprom_read_error;
  }

#else // !EEPROM_SETTINGS

  bool EEPROM::Store_Settings() { SERIAL_LM(ER, "EEPROM disabled"); return false; }

#endif // EEPROM_SETTINGS

/**
 * M502 - Reset Configuration
 */
void EEPROM::Factory_Settings() {
  const float     tmp1[] = DEFAULT_AXIS_STEPS_PER_UNIT,
                  tmp2[] = DEFAULT_MAX_FEEDRATE;
  const uint32_t  tmp3[] = DEFAULT_MAX_ACCELERATION,
                  tmp4[] = DEFAULT_RETRACT_ACCELERATION;
  const float     tmp5[] = DEFAULT_EJERK,
                  tmp6[] = DEFAULT_Kp,
                  tmp7[] = DEFAULT_Ki,
                  tmp8[] = DEFAULT_Kd,
                  tmp9[] = DEFAULT_Kc;

  #if ENABLED(HOTEND_OFFSET_X) && ENABLED(HOTEND_OFFSET_Y) && ENABLED(HOTEND_OFFSET_Z)
    constexpr float tmp10[XYZ][4] = {
      HOTEND_OFFSET_X,
      HOTEND_OFFSET_Y,
      HOTEND_OFFSET_Z
    };
  #else
    constexpr float tmp10[XYZ][HOTENDS] = { 0.0 };
  #endif

  #if MB(ALLIGATOR) || MB(ALLIGATOR_V3)
    float tmp11[] = MOTOR_CURRENT;
    for (int8_t i = 0; i < 3 + DRIVER_EXTRUDERS; i++)
      motor_current[i] = tmp11[i];
  #endif

  LOOP_XYZE_N(i) {
    planner.axis_steps_per_mm[i]          = tmp1[i < COUNT(tmp1) ? i : COUNT(tmp1) - 1];
    planner.max_feedrate_mm_s[i]          = tmp2[i < COUNT(tmp2) ? i : COUNT(tmp2) - 1];
    planner.max_acceleration_mm_per_s2[i] = tmp3[i < COUNT(tmp3) ? i : COUNT(tmp3) - 1];
  }

  for (int8_t i = 0; i < EXTRUDERS; i++) {
    planner.retract_acceleration[i]       = tmp4[i < COUNT(tmp4) ? i : COUNT(tmp4) - 1];
    planner.max_jerk[E_AXIS + i]          = tmp5[i < COUNT(tmp5) ? i : COUNT(tmp5) - 1];
  }

  static_assert(
    tmp10[X_AXIS][0] == 0 && tmp10[Y_AXIS][0] == 0 && tmp10[Z_AXIS][0] == 0,
    "Offsets for the first hotend must be 0.0."
  );
  LOOP_XYZ(i) {
    HOTEND_LOOP() hotend_offset[i][h] = tmp10[i][h];
  }

  planner.acceleration = DEFAULT_ACCELERATION;
  planner.travel_acceleration = DEFAULT_TRAVEL_ACCELERATION;
  planner.min_feedrate_mm_s = DEFAULT_MINIMUMFEEDRATE;
  planner.min_segment_time = DEFAULT_MINSEGMENTTIME;
  planner.min_travel_feedrate_mm_s = DEFAULT_MINTRAVELFEEDRATE;
  planner.max_jerk[X_AXIS] = DEFAULT_XJERK;
  planner.max_jerk[Y_AXIS] = DEFAULT_YJERK;
  planner.max_jerk[Z_AXIS] = DEFAULT_ZJERK;

  #if ENABLED(ENABLE_LEVELING_FADE_HEIGHT)
    planner.z_fade_height = 0.0;
  #endif

  #if ENABLED(WORKSPACE_OFFSETS)
    ZERO(home_offset);
  #endif

  #if HAS_LEVELING
    reset_bed_level();
  #endif

  #if HAS_BED_PROBE
    zprobe_zoffset = Z_PROBE_OFFSET_FROM_NOZZLE;
  #endif

  #if MECH(DELTA)
    deltaParams.Init();
  #endif

  #if ENABLED(ULTIPANEL)
    lcd_preheat_hotend_temp[0] = PREHEAT_1_TEMP_HOTEND;
    lcd_preheat_hotend_temp[1] = PREHEAT_2_TEMP_HOTEND;
    lcd_preheat_hotend_temp[2] = PREHEAT_3_TEMP_HOTEND;
    lcd_preheat_bed_temp[0] = PREHEAT_1_TEMP_BED;
    lcd_preheat_bed_temp[1] = PREHEAT_2_TEMP_BED;
    lcd_preheat_bed_temp[2] = PREHEAT_3_TEMP_BED;
    lcd_preheat_fan_speed[0] = PREHEAT_1_FAN_SPEED;
    lcd_preheat_fan_speed[1] = PREHEAT_2_FAN_SPEED;
    lcd_preheat_fan_speed[2] = PREHEAT_3_FAN_SPEED;
  #endif

  #if HAS(LCD_CONTRAST)
    lcd_contrast = DEFAULT_LCD_CONTRAST;
  #endif

  #if ENABLED(PIDTEMP)
    HOTEND_LOOP() {
      PID_PARAM(Kp, h) = tmp6[h];
      PID_PARAM(Ki, h) = tmp7[h];
      PID_PARAM(Kd, h) = tmp8[h];
      PID_PARAM(Kc, h) = tmp9[h];
    }
    #if ENABLED(PID_ADD_EXTRUSION_RATE)
      lpq_len = 20; // default last-position-queue size
    #endif
  #endif // PIDTEMP

  #if ENABLED(PIDTEMPBED)
    thermalManager.bedKp = DEFAULT_bedKp;
    thermalManager.bedKi = DEFAULT_bedKi;
    thermalManager.bedKd = DEFAULT_bedKd;
  #endif

  #if ENABLED(PIDTEMPCHAMBER)
    thermalManager.chamberKp = DEFAULT_chamberKp;
    thermalManager.chamberKi = DEFAULT_chamberKi;
    thermalManager.chamberKd = DEFAULT_chamberKd;
  #endif

  #if ENABLED(PIDTEMPCOOLER)
    thermalManager.coolerKp = DEFAULT_coolerKp;
    thermalManager.coolerKi = DEFAULT_coolerKi;
    thermalManager.coolerKd = DEFAULT_coolerKd;
  #endif

  #if ENABLED(FWRETRACT)
    autoretract_enabled = false;
    retract_length = RETRACT_LENGTH;
    #if EXTRUDERS > 1
      retract_length_swap = RETRACT_LENGTH_SWAP;
    #endif
    retract_feedrate = RETRACT_FEEDRATE;
    retract_zlift = RETRACT_ZLIFT;
    retract_recover_length = RETRACT_RECOVER_LENGTH;
    #if EXTRUDERS > 1
      retract_recover_length_swap = RETRACT_RECOVER_LENGTH_SWAP;
    #endif
    retract_recover_feedrate = RETRACT_RECOVER_FEEDRATE;
  #endif

  #if ENABLED(VOLUMETRIC_DEFAULT_ON)
    volumetric_enabled = true;
  #else
    volumetric_enabled = false;
  #endif

  for (uint8_t q = 0; q < COUNT(filament_size); q++)
    filament_size[q] = DEFAULT_NOMINAL_FILAMENT_DIA;

  endstops.enable_globally(
    #if ENABLED(ENDSTOPS_ONLY_FOR_HOMING)
      (false)
    #else
      (true)
    #endif
  );

  #if ENABLED(IDLE_OOZING_PREVENT)
    IDLE_OOZING_enabled = true;
  #endif

  #if ENABLED(HAVE_TMC2130)
    #if ENABLED(X_IS_TMC2130)
      stepperX.setCurrent(X_CURRENT, R_SENSE, HOLD_MULTIPLIER);
    #endif
    #if ENABLED(Y_IS_TMC2130)
      stepperY.setCurrent(Y_CURRENT, R_SENSE, HOLD_MULTIPLIER);
    #endif
    #if ENABLED(Z_IS_TMC2130)
      stepperZ.setCurrent(Z_CURRENT, R_SENSE, HOLD_MULTIPLIER);
    #endif
    #if ENABLED(X2_IS_TMC2130)
      stepperX2.setCurrent(X2_CURRENT, R_SENSE, HOLD_MULTIPLIER);
    #endif
    #if ENABLED(Y2_IS_TMC2130)
      stepperY2.setCurrent(Y2_CURRENT, R_SENSE, HOLD_MULTIPLIER);
    #endif
    #if ENABLED(Z2_IS_TMC2130)
      stepperZ2.setCurrent(Z2_MAX_CURRENT, R_SENSE, HOLD_MULTIPLIER);
    #endif
    #if ENABLED(E0_IS_TMC2130)
      stepperE0.setCurrent(E0_CURRENT, R_SENSE, HOLD_MULTIPLIER);
    #endif
    #if ENABLED(E1_IS_TMC2130)
      stepperE1.setCurrent(E1_CURRENT, R_SENSE, HOLD_MULTIPLIER);
    #endif
    #if ENABLED(E2_IS_TMC2130)
      stepperE2.setCurrent(E2_CURRENT, R_SENSE, HOLD_MULTIPLIER);
    #endif
    #if ENABLED(E3_IS_TMC2130)
      stepperE3.setCurrent(E3_CURRENT, R_SENSE, HOLD_MULTIPLIER);
    #endif
    #if ENABLED(E4_IS_TMC2130)
      stepperE4.setCurrent(E4_CURRENT, R_SENSE, HOLD_MULTIPLIER);
    #endif
    #if ENABLED(E5_IS_TMC2130)
      stepperE5.setCurrent(E5_CURRENT, R_SENSE, HOLD_MULTIPLIER);
    #endif
  #endif

  #if ENABLED(LIN_ADVANCE)
    planner.extruder_advance_k = LIN_ADVANCE_K;
    planner.advance_ed_ratio = LIN_ADVANCE_E_D_RATIO;
  #endif

  Postprocess();

  SERIAL_LM(ECHO, "Hardcoded Default Settings Loaded");
}

#if DISABLED(DISABLE_M503)

  #define CONFIG_MSG_START(str) do{ if (!forReplay) SERIAL_S(CFG); SERIAL_EM(str); }while(0)

  /**
   * M503 - Print Configuration
   */
  void EEPROM::Print_Settings(bool forReplay) {
    // Always have this function, even with EEPROM_SETTINGS disabled, the current values will be shown

    /**
     * Announce current units, in case inches are being displayed
     */
    #if ENABLED(INCH_MODE_SUPPORT)
      extern float linear_unit_factor, volumetric_unit_factor;
      #define LINEAR_UNIT(N) ((N) / linear_unit_factor)
      #define VOLUMETRIC_UNIT(N) ((N) / (volumetric_enabled ? volumetric_unit_factor : linear_unit_factor))
      SERIAL_SM(CFG, "  G2");
      SERIAL_C(linear_unit_factor == 1.0 ? '1' : '0');
      SERIAL_M(" ; Units in ");
      SERIAL_PS(linear_unit_factor == 1.0 ? PSTR("mm\n") : PSTR("inches\n"));
    #else
      #define LINEAR_UNIT(N) N
      #define VOLUMETRIC_UNIT(N) N
      SERIAL_LM(CFG, "  G21 ; Units in mm");
    #endif
    SERIAL_E;

    #if ENABLED(ULTIPANEL)

      // Temperature units - for Ultipanel temperature options

      #if ENABLED(TEMPERATURE_UNITS_SUPPORT)
        extern TempUnit input_temp_units;
        extern float to_temp_units(const float &f);
        #define TEMP_UNIT(N) to_temp_units(N)
        SERIAL_SM(CFG, "  M149 ");
        SERIAL_C(input_temp_units == TEMPUNIT_K ? 'K' : input_temp_units == TEMPUNIT_F ? 'F' : 'C');
        SERIAL_M(" ; Units in ");
        SERIAL_PS(input_temp_units == TEMPUNIT_K ? PSTR("Kelvin\n") : input_temp_units == TEMPUNIT_F ? PSTR("Fahrenheit\n") : PSTR("Celsius\n"));
      #else
        #define TEMP_UNIT(N) N
        SERIAL_LM(CFG, "  M149 C ; Units in Celsius\n");
      #endif
      SERIAL_E;

    #endif

    CONFIG_MSG_START("Steps per unit:");
    SERIAL_SMV(CFG, "  M92 X", LINEAR_UNIT(planner.axis_steps_per_mm[X_AXIS]), 3);
    SERIAL_MV(" Y", LINEAR_UNIT(planner.axis_steps_per_mm[Y_AXIS]), 3);
    SERIAL_MV(" Z", LINEAR_UNIT(planner.axis_steps_per_mm[Z_AXIS]), 3);
    #if EXTRUDERS == 1
      SERIAL_MV(" T0 E", VOLUMETRIC_UNIT(planner.axis_steps_per_mm[E_AXIS]), 3);
    #endif
    SERIAL_E;
    #if EXTRUDERS > 1
      for (int8_t i = 0; i < EXTRUDERS; i++) {
        SERIAL_SMV(CFG, "  M92 T", i);
        SERIAL_EMV(" E", VOLUMETRIC_UNIT(planner.axis_steps_per_mm[E_AXIS + i]), 3);
      }
    #endif // EXTRUDERS > 1

    CONFIG_MSG_START("Maximum feedrates (units/s):");
    SERIAL_SMV(CFG, "  M203 X", LINEAR_UNIT(planner.max_feedrate_mm_s[X_AXIS]), 3);
    SERIAL_MV(" Y", LINEAR_UNIT(planner.max_feedrate_mm_s[Y_AXIS]), 3);
    SERIAL_MV(" Z", LINEAR_UNIT(planner.max_feedrate_mm_s[Z_AXIS]), 3);
    #if EXTRUDERS == 1
      SERIAL_MV(" T0 E", VOLUMETRIC_UNIT(planner.max_feedrate_mm_s[E_AXIS]), 3);
    #endif
    SERIAL_E;
    #if EXTRUDERS > 1
      for (int8_t i = 0; i < EXTRUDERS; i++) {
        SERIAL_SMV(CFG, "  M203 T", i);
        SERIAL_EMV(" E", VOLUMETRIC_UNIT(planner.max_feedrate_mm_s[E_AXIS + i]), 3);
      }
    #endif // EXTRUDERS > 1

    CONFIG_MSG_START("Maximum Acceleration (units/s2):");
    SERIAL_SMV(CFG, "  M201 X", LINEAR_UNIT(planner.max_acceleration_mm_per_s2[X_AXIS]));
    SERIAL_MV(" Y", LINEAR_UNIT(planner.max_acceleration_mm_per_s2[Y_AXIS]));
    SERIAL_MV(" Z", LINEAR_UNIT(planner.max_acceleration_mm_per_s2[Z_AXIS]));
    #if EXTRUDERS == 1
      SERIAL_MV(" T0 E", VOLUMETRIC_UNIT(planner.max_acceleration_mm_per_s2[E_AXIS]));
    #endif
    SERIAL_E;
    #if EXTRUDERS > 1
      for (int8_t i = 0; i < EXTRUDERS; i++) {
        SERIAL_SMV(CFG, "  M201 T", i);
        SERIAL_EMV(" E", VOLUMETRIC_UNIT(planner.max_acceleration_mm_per_s2[E_AXIS + i]));
      }
    #endif // EXTRUDERS > 1

    CONFIG_MSG_START("Acceleration (units/s2): P<print_accel> V<travel_accel> T* R<retract_accel>");
    SERIAL_SMV(CFG,"  M204 P", LINEAR_UNIT(planner.acceleration), 3);
    SERIAL_MV(" V", LINEAR_UNIT(planner.travel_acceleration), 3);
    #if EXTRUDERS == 1
      SERIAL_MV(" T0 R", LINEAR_UNIT(planner.retract_acceleration[0]), 3);
    #endif
    SERIAL_E;
    #if EXTRUDERS > 1
      for (int8_t i = 0; i < EXTRUDERS; i++) {
        SERIAL_SMV(CFG, "  M204 T", i);
        SERIAL_EMV(" R", LINEAR_UNIT(planner.retract_acceleration[i]), 3);
      }
    #endif

    CONFIG_MSG_START("Advanced variables: S<min_feedrate> V<min_travel_feedrate> B<min_segment_time_ms> X<max_xy_jerk> Z<max_z_jerk> T* E<max_e_jerk>");
    SERIAL_SMV(CFG, "  M205 S", LINEAR_UNIT(planner.min_feedrate_mm_s), 3);
    SERIAL_MV(" V", LINEAR_UNIT(planner.min_travel_feedrate_mm_s), 3);
    SERIAL_MV(" B", planner.min_segment_time);
    SERIAL_MV(" X", LINEAR_UNIT(planner.max_jerk[X_AXIS]), 3);
    SERIAL_MV(" Y", LINEAR_UNIT(planner.max_jerk[Y_AXIS]), 3);
    SERIAL_MV(" Z", LINEAR_UNIT(planner.max_jerk[Z_AXIS]), 3);
    #if EXTRUDERS == 1
      SERIAL_MV(" T0 E", LINEAR_UNIT(planner.max_jerk[E_AXIS]), 3);
    #endif
    SERIAL_E;
    #if (EXTRUDERS > 1)
      for(int8_t i = 0; i < EXTRUDERS; i++) {
        SERIAL_SMV(CFG, "  M205 T", i);
        SERIAL_EMV(" E" , LINEAR_UNIT(planner.max_jerk[E_AXIS + i]), 3);
      }
    #endif

    #if ENABLED(WORKSPACE_OFFSETS)
      CONFIG_MSG_START("Home offset:");
      SERIAL_SMV(CFG, "  M206 X", LINEAR_UNIT(home_offset[X_AXIS]), 3);
      SERIAL_MV(" Y", LINEAR_UNIT(home_offset[Y_AXIS]), 3);
      SERIAL_EMV(" Z", LINEAR_UNIT(home_offset[Z_AXIS]), 3);
    #endif

    #if HOTENDS > 1
      CONFIG_MSG_START("Hotend offset (mm):");
      for (int8_t h = 1; h < HOTENDS; h++) {
        SERIAL_SMV(CFG, "  M218 H", h);
        SERIAL_MV(" X", LINEAR_UNIT(hotend_offset[X_AXIS][h]), 3);
        SERIAL_MV(" Y", LINEAR_UNIT(hotend_offset[Y_AXIS][h]), 3);
        SERIAL_EMV(" Z", LINEAR_UNIT(hotend_offset[Z_AXIS][h]), 3);
      }
    #endif

    #if HAS(LCD_CONTRAST)
      CONFIG_MSG_START("LCD Contrast:");
      SERIAL_LMV(CFG, "  M250 C", lcd_contrast);
    #endif

    #if ENABLED(MESH_BED_LEVELING)

      CONFIG_MSG_START("Mesh Bed Leveling:");
      SERIAL_SMV(CFG, "  M420 S", mbl.has_mesh() ? 1 : 0);
      #if ENABLED(ENABLE_LEVELING_FADE_HEIGHT)
        SERIAL_MV(" Z", LINEAR_UNIT(planner.z_fade_height));
      #endif
      SERIAL_E;

      for (uint8_t py = 0; py < GRID_MAX_POINTS_Y; py++) {
        for (uint8_t px = 0; px < GRID_MAX_POINTS_X; px++) {
          SERIAL_SMV(CFG, "  G29 S3 X", (int)px + 1);
          SERIAL_MV(" Y", (int)py + 1);
          SERIAL_EMV(" Z", LINEAR_UNIT(mbl.z_values[px][py]), 5);
        }
      }

    #elif HAS_ABL

      CONFIG_MSG_START("Auto Bed Leveling:");
      SERIAL_SMV(CFG, "  M320 S", planner.abl_enabled ? 1 : 0);
      #if ENABLED(ENABLE_LEVELING_FADE_HEIGHT)
        SERIAL_MV(" Z", LINEAR_UNIT(planner.z_fade_height));
      #endif
      SERIAL_E;

    #endif

    #if HEATER_USES_AD595
      CONFIG_MSG_START("AD595 Offset and Gain:");
      for (int8_t h = 0; h < HOTENDS; h++) {
        SERIAL_SMV(CFG, "  M595 H", h);
        SERIAL_MV(" O", ad595_offset[h]);
        SERIAL_EMV(", S", ad595_gain[h]);
      }
    #endif // HEATER_USES_AD595

    #if MECH(DELTA)

      CONFIG_MSG_START("Endstop adjustment:");
      SERIAL_SM(CFG, "  M666");
      SERIAL_MV(" X", LINEAR_UNIT(deltaParams.endstop_adj[A_AXIS]));
      SERIAL_MV(" Y", LINEAR_UNIT(deltaParams.endstop_adj[B_AXIS]));
      SERIAL_MV(" Z", LINEAR_UNIT(deltaParams.endstop_adj[C_AXIS]));
      SERIAL_E;

      CONFIG_MSG_START("Geometry adjustment: ABC=TOWER_DIAGROD_ADJ, IJK=TOWER_RADIUS_ADJ, UVW=TOWER_POSITION_ADJ");
      CONFIG_MSG_START("                     R=Delta Radius, D=DELTA_DIAGONAL_ROD, S=DELTA_SEGMENTS_PER_SECOND");
      CONFIG_MSG_START("                     O=DELTA_PRINTABLE_RADIUS, H=DELTA_HEIGHT");
      SERIAL_SM(CFG, "  M666");
      SERIAL_MV(" A", LINEAR_UNIT(deltaParams.diagonal_rod_adj[0]), 3);
      SERIAL_MV(" B", LINEAR_UNIT(deltaParams.diagonal_rod_adj[1]), 3);
      SERIAL_MV(" C", LINEAR_UNIT(deltaParams.diagonal_rod_adj[2]), 3);
      SERIAL_MV(" I", deltaParams.tower_radius_adj[0], 3);
      SERIAL_MV(" J", deltaParams.tower_radius_adj[1], 3);
      SERIAL_MV(" K", deltaParams.tower_radius_adj[2], 3);
      SERIAL_MV(" U", LINEAR_UNIT(deltaParams.tower_pos_adj[0]), 3);
      SERIAL_MV(" V", LINEAR_UNIT(deltaParams.tower_pos_adj[1]), 3);
      SERIAL_MV(" W", LINEAR_UNIT(deltaParams.tower_pos_adj[2]), 3);
      SERIAL_MV(" R", LINEAR_UNIT(deltaParams.delta_radius));
      SERIAL_MV(" D", LINEAR_UNIT(deltaParams.diagonal_rod));
      SERIAL_MV(" S", deltaParams.segments_per_second);
      SERIAL_MV(" O", LINEAR_UNIT(deltaParams.print_radius));
      SERIAL_MV(" H", LINEAR_UNIT(deltaParams.delta_height), 3);
      SERIAL_E;

    #elif ENABLED(Z_TWO_ENDSTOPS)

      CONFIG_MSG_START("Z2 Endstop adjustement:");
      SERIAL_LMV(CFG, "  M666 Z", LINEAR_UNIT(z2_endstop_adj));

    #endif // DELTA

    /**
     * Auto Bed Leveling
     */
    #if HAS_BED_PROBE
      CONFIG_MSG_START("Z Probe offset:");
      SERIAL_LMV(CFG, "  M666 P", LINEAR_UNIT(zprobe_zoffset));
    #endif

    #if ENABLED(ULTIPANEL)
      CONFIG_MSG_START("Material heatup parameters:");
      for (int8_t i = 0; i < COUNT(lcd_preheat_hotend_temp); i++) {
        SERIAL_SMV(CFG, "  M145 S", i);
        SERIAL_MV(" H", TEMP_UNIT(lcd_preheat_hotend_temp[i]));
        SERIAL_MV(" B", TEMP_UNIT(lcd_preheat_bed_temp[i]));
        SERIAL_MV(" F", lcd_preheat_fan_speed[i]);
        SERIAL_E;
      }
    #endif // ULTIPANEL

    #if ENABLED(PIDTEMP) || ENABLED(PIDTEMPBED) || ENABLED(PIDTEMPCHAMBER) || ENABLED(PIDTEMPCOOLER)
      CONFIG_MSG_START("PID settings:");
      #if ENABLED(PIDTEMP)
        #if HOTENDS == 1
          SERIAL_SM(CFG, "  M301 H0");
          SERIAL_MV(" P", PID_PARAM(Kp, 0));
          SERIAL_MV(" I", PID_PARAM(Ki, 0));
          SERIAL_MV(" D", PID_PARAM(Kd, 0));
          #if ENABLED(PID_ADD_EXTRUSION_RATE)
            SERIAL_MV(" C", PID_PARAM(Kc, 0));
          #endif
          SERIAL_E;
          #if ENABLED(PID_ADD_EXTRUSION_RATE)
            SERIAL_LMV(CFG, "  M301 L", lpq_len);
          #endif
        #elif HOTENDS > 1
          for (int8_t h = 0; h < HOTENDS; h++) {
            SERIAL_SMV(CFG, "  M301 H", h);
            SERIAL_MV(" P", PID_PARAM(Kp, h));
            SERIAL_MV(" I", PID_PARAM(Ki, h));
            SERIAL_MV(" D", PID_PARAM(Kd, h));
            #if ENABLED(PID_ADD_EXTRUSION_RATE)
              SERIAL_MV(" C", PID_PARAM(Kc, h));
            #endif
            SERIAL_E;
          }
          #if ENABLED(PID_ADD_EXTRUSION_RATE)
            SERIAL_LMV(CFG, "  M301 L", lpq_len);
          #endif
        #endif
      #endif
      #if ENABLED(PIDTEMPBED)
        SERIAL_SMV(CFG, "  M304 P", thermalManager.bedKp);
        SERIAL_MV(" I", thermalManager.bedKi);
        SERIAL_EMV(" D", thermalManager.bedKd);
      #endif
      #if ENABLED(PIDTEMPCHAMBER)
        SERIAL_SMV(CFG, "  M305 P", thermalManager.chamberKp);
        SERIAL_MV(" I", thermalManager.chamberKi);
        SERIAL_EMV(" D", thermalManager.chamberKd);
      #endif
      #if ENABLED(PIDTEMPCOOLER)
        SERIAL_SMV(CFG, "  M306 P", thermalManager.coolerKp);
        SERIAL_MV(" I", thermalManager.coolerKi);
        SERIAL_EMV(" D", thermalManager.coolerKd);
      #endif
    #endif

    #if ENABLED(FWRETRACT)
      CONFIG_MSG_START("Retract: S=Length (mm) F:Speed (mm/m) Z: ZLift (mm)");
      SERIAL_SMV(CFG, "  M207 S", retract_length);
      #if EXTRUDERS > 1
        SERIAL_MV(" W", retract_length_swap);
      #endif
      SERIAL_MV(" F", retract_feedrate * 60);
      SERIAL_EMV(" Z", retract_zlift);

      CONFIG_MSG_START("Recover: S=Extra length (mm) F:Speed (mm/m)");
      SERIAL_SMV(CFG, "  M208 S", retract_recover_length);
      #if EXTRUDERS > 1
        SERIAL_MV(" W", retract_recover_length_swap);
      #endif
      SERIAL_MV(" F", retract_recover_feedrate * 60);

      CONFIG_MSG_START("Auto-Retract: S=0 to disable, 1 to interpret extrude-only moves as retracts or recoveries");
      SERIAL_LMV(CFG, "  M209 S", autoretract_enabled ? 1 : 0);
    #endif // FWRETRACT

    /**
     * Volumetric extrusion M200
     */
    if (!forReplay) {
      SERIAL_SM(CFG, "Filament settings:");
      if (volumetric_enabled)
        SERIAL_E;
      else
        SERIAL_EM(" Disabled");
    }
    #if EXTRUDERS == 1
      SERIAL_LMV(CFG, "  M200 T0 D", filament_size[0], 3);
    #endif
    #if EXTRUDERS > 1
      for (uint8_t i = 0; i < EXTRUDERS; i++) {
        SERIAL_SMV(CFG, "  M200 T", (int)i);
        SERIAL_EMV(" D", filament_size[i], 3);
      }
    #endif

    /**
     * Alligator current drivers M906
     */
    #if MB(ALLIGATOR) || MB(ALLIGATOR_V3)
      CONFIG_MSG_START("Motor current:");
      SERIAL_SMV(CFG, "  M906 X", motor_current[X_AXIS], 2);
      SERIAL_MV(" Y", motor_current[Y_AXIS], 2);
      SERIAL_MV(" Z", motor_current[Z_AXIS], 2);
      #if EXTRUDERS == 1
        SERIAL_MV(" T0 E", motor_current[E_AXIS], 2);
      #endif
      SERIAL_E;
      #if DRIVER_EXTRUDERS > 1
        for (uint8_t i = 0; i < DRIVER_EXTRUDERS; i++) {
          SERIAL_SMV(CFG, "  M906 T", i);
          SERIAL_EMV(" E", motor_current[E_AXIS + i], 2);
        }
      #endif // DRIVER_EXTRUDERS > 1
    #endif // ALLIGATOR

    /**
     * TMC2130 stepper driver current
     */
    #if ENABLED(HAVE_TMC2130)
      CONFIG_MSG_START("Stepper driver current:");
      SERIAL_SM(CFG, "  M906");
      #if ENABLED(X_IS_TMC2130)
        SERIAL_MV(" X", stepperX.getCurrent());
      #endif
      #if ENABLED(Y_IS_TMC2130)
        SERIAL_MV(" Y", stepperY.getCurrent());
      #endif
      #if ENABLED(Z_IS_TMC2130)
        SERIAL_MV(" Z", stepperZ.getCurrent());
      #endif
      #if ENABLED(X2_IS_TMC2130)
        SERIAL_MV(" X2", stepperX2.getCurrent());
      #endif
      #if ENABLED(Y2_IS_TMC2130)
        SERIAL_MV(" Y2", stepperY2.getCurrent());
      #endif
      #if ENABLED(Z2_IS_TMC2130)
        SERIAL_MV(" Z2", stepperZ2.getCurrent());
      #endif
      #if ENABLED(E0_IS_TMC2130)
        SERIAL_MV(" E0", stepperE0.getCurrent());
      #endif
      #if ENABLED(E1_IS_TMC2130)
        SERIAL_MV(" E1", stepperE1.getCurrent());
      #endif
      #if ENABLED(E2_IS_TMC2130)
        SERIAL_MV(" E2", stepperE2.getCurrent());
      #endif
      #if ENABLED(E3_IS_TMC2130)
        SERIAL_MV(" E3", stepperE3.getCurrent());
      #endif
      SERIAL_E;
      #if ENABLED(E4_IS_TMC2130)
        SERIAL_MV(" E4", stepperE4.getCurrent());
      #endif
      SERIAL_E;
      #if ENABLED(E5_IS_TMC2130)
        SERIAL_MV(" E5", stepperE5.getCurrent());
      #endif
      SERIAL_E;
    #endif

    /**
     * Linear Advance
     */
    #if ENABLED(LIN_ADVANCE)
      CONFIG_MSG_START("Linear Advance:");
      SERIAL_SMV(CFG, "  M900 K", planner.extruder_advance_k);
      SERIAL_EMV(" R", planner.advance_ed_ratio);
    #endif

    #if ENABLED(SDSUPPORT)
      card.PrintSettings();
    #endif

  }

#endif // !DISABLE_M503
