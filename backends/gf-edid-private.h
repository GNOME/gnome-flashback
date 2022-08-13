/*
 * Copyright 2007, 2008, Red Hat, Inc.
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
 * Author: Soren Sandmann <sandmann@redhat.com>
 */

#ifndef GF_EDID_PRIVATE_H
#define GF_EDID_PRIVATE_H

#include <glib.h>
#include <stdint.h>

G_BEGIN_DECLS

typedef struct _GfEdidInfo GfEdidInfo;
typedef struct _GfEdidTiming GfEdidTiming;
typedef struct _GfEdidDetailedTiming GfEdidDetailedTiming;
typedef struct _GfEdidHdrStaticMetadata GfEdidHdrStaticMetadata;

typedef enum
{
  GF_EDID_INTERFACE_UNDEFINED,
  GF_EDID_INTERFACE_DVI,
  GF_EDID_INTERFACE_HDMI_A,
  GF_EDID_INTERFACE_HDMI_B,
  GF_EDID_INTERFACE_MDDI,
  GF_EDID_INTERFACE_DISPLAY_PORT
} GfEdidInterface;

typedef enum
{
  GF_EDID_COLOR_TYPE_UNDEFINED,
  GF_EDID_COLOR_TYPE_MONOCHROME,
  GF_EDID_COLOR_TYPE_RGB,
  GF_EDID_COLOR_TYPE_OTHER_COLOR
} GfEdidColorType;

typedef enum
{
  GF_EDID_STEREO_TYPE_NO_STEREO,
  GF_EDID_STEREO_TYPE_FIELD_RIGHT,
  GF_EDID_STEREO_TYPE_FIELD_LEFT,
  GF_EDID_STEREO_TYPE_TWO_WAY_RIGHT_ON_EVEN,
  GF_EDID_STEREO_TYPE_TWO_WAY_LEFT_ON_EVEN,
  GF_EDID_STEREO_TYPE_FOUR_WAY_INTERLEAVED,
  GF_EDID_STEREO_TYPE_SIDE_BY_SIDE
} GfEdidStereoType;

typedef enum
{
  GF_EDID_COLORIMETRY_XVYCC601    = (1 << 0),
  GF_EDID_COLORIMETRY_XVYCC709    = (1 << 1),
  GF_EDID_COLORIMETRY_SYCC601     = (1 << 2),
  GF_EDID_COLORIMETRY_OPYCC601    = (1 << 3),
  GF_EDID_COLORIMETRY_OPRGB       = (1 << 4),
  GF_EDID_COLORIMETRY_BT2020CYCC  = (1 << 5),
  GF_EDID_COLORIMETRY_BT2020YCC   = (1 << 6),
  GF_EDID_COLORIMETRY_BT2020RGB   = (1 << 7),
  GF_EDID_COLORIMETRY_ST2113RGB   = (1 << 14),
  GF_EDID_COLORIMETRY_ICTCP       = (1 << 15),
} GfEdidColorimetry;

typedef enum
{
  GF_EDID_TF_TRADITIONAL_GAMMA_SDR = (1 << 0),
  GF_EDID_TF_TRADITIONAL_GAMMA_HDR = (1 << 1),
  GF_EDID_TF_PQ                    = (1 << 2),
  GF_EDID_TF_HLG                   = (1 << 3),
} GfEdidTransferFunction;

typedef enum
{
  GF_EDID_STATIC_METADATA_TYPE1 = 0,
} GfEdidStaticMetadataType;

struct _GfEdidTiming
{
  int width;
  int height;
  int frequency;
};

struct _GfEdidDetailedTiming
{
  int        pixel_clock;
  int        h_addr;
  int        h_blank;
  int        h_sync;
  int        h_front_porch;
  int        v_addr;
  int        v_blank;
  int        v_sync;
  int        v_front_porch;
  int        width_mm;
  int        height_mm;
  int        right_border;
  int        top_border;
  int        interlaced;
  GfEdidStereoType stereo;

  int        digital_sync;
  union
  {
    struct
    {
      int    bipolar;
      int    serrations;
      int    sync_on_green;
    } analog;

    struct
    {
      int    composite;
      int    serrations;
      int    negative_vsync;
      int    negative_hsync;
    } digital;
  } connector;
};

struct _GfEdidHdrStaticMetadata
{
  int                      available;
  int                      max_luminance;
  int                      min_luminance;
  int                      max_fal;
  GfEdidTransferFunction   tf;
  GfEdidStaticMetadataType sm;
};

struct _GfEdidInfo
{
  int            checksum;
  char           manufacturer_code[4];
  int            product_code;
  unsigned int   serial_number;

  int            production_week;       /* -1 if not specified */
  int            production_year;       /* -1 if not specified */
  int            model_year;            /* -1 if not specified */

  int            major_version;
  int            minor_version;

  int            is_digital;

  union
  {
    struct
    {
      int        bits_per_primary;
      GfEdidInterface interface;
      int        rgb444;
      int        ycrcb444;
      int        ycrcb422;
    } digital;

    struct
    {
      double     video_signal_level;
      double     sync_signal_level;
      double     total_signal_level;

      int        blank_to_black;

      int        separate_hv_sync;
      int        composite_sync_on_h;
      int        composite_sync_on_green;
      int        serration_on_vsync;
      GfEdidColorType color_type;
    } analog;
  } connector;

  int            width_mm;              /* -1 if not specified */
  int            height_mm;             /* -1 if not specified */
  double         aspect_ratio;          /* -1.0 if not specififed */

  double         gamma;                 /* -1.0 if not specified */

  int            standby;
  int            suspend;
  int            active_off;

  int            srgb_is_standard;
  int            preferred_timing_includes_native;
  int            continuous_frequency;

  double         red_x;
  double         red_y;
  double         green_x;
  double         green_y;
  double         blue_x;
  double         blue_y;
  double         white_x;
  double         white_y;

  GfEdidTiming   established[24];       /* Terminated by 0x0x0 */
  GfEdidTiming   standard[8];

  int            n_detailed_timings;
  GfEdidDetailedTiming detailed_timings[4]; /* If monitor has a preferred
                                             * mode, it is the first one
                                             * (whether it has, is
                                             * determined by the
                                             * preferred_timing_includes
                                             * bit.
                                             */

  /* Optional product description */
  char           dsc_serial_number[14];
  char           dsc_product_name[14];
  char           dsc_string[14];        /* Unspecified ASCII data */

  GfEdidColorimetry colorimetry;
  GfEdidHdrStaticMetadata hdr_static_metadata;
};

GfEdidInfo *gf_edid_info_new_parse (const uint8_t *data);

G_END_DECLS

#endif
