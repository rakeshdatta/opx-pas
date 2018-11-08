/*
 * Copyright (c) 2018 Dell EMC.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * THIS CODE IS PROVIDED ON AN  *AS IS* BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT
 *  LIMITATION ANY IMPLIED WARRANTIES OR CONDITIONS OF TITLE, FITNESS
 * FOR A PARTICULAR PURPOSE, MERCHANTABLITY OR NON-INFRINGEMENT.
 *
 * See the Apache Version 2.0 License for specific language governing
 * permissions and limitations under the License.
 */

/**************************************************************************
 * @file pas_media_properties_utils.c
 *
 * @brief This file contains source code for utilities involved in qualifying media
 ***************************************************************************/

#include "private/pas_log.h"
#include "private/pas_media.h"
#include "private/pas_media_sdi_wrapper.h"
#include "private/pas_data_store.h"
#include "private/pald.h"
#include "private/dn_pas.h"
#include "private/pas_event.h"
#include "private/pas_utils.h"
#include <stdlib.h>
#include "std_utils.h"

#define QSFP_PLUS_40G_BIDI_PART_NUMBER "AFBR-79EBPZ-CS"

/* Function to construct the display string from a series of attibutes */
/* Consumer services will add extra string if QSA detected  */


static void pas_media_construct_base_display_string (dn_pas_basic_media_info_t*
                                                   media_info, char separator,
                                                      char* output, size_t len)

{
    char far_end_str[10]  = {0};
    char speed_str[20]    = {0};
    char length_str[10]   = {0};
    char media_if_str[20] = {0};
    char prefix_str[5]    = {0};
    char postfix_str[5]   = {0};

    const char fc_str[]   = "-FC";
    const char* trans_str =
            pas_media_get_transceiver_type_display_string(media_info->transceiver_type);
    const char* media_interface_str =
            pas_media_get_media_interface_disp_string (media_info->media_interface);
    const char* media_interface_qualifier_str =
            pas_media_get_media_interface_qualifier_disp_string(
                   media_info->media_interface_qualifier);

    uint_t far_end = pas_media_map_get_breakout_far_end_val(
            media_info->capability_list[0].breakout_mode);
    uint_t brk_speed = pas_media_map_get_speed_as_uint_mbps(
            media_info->capability_list[0].breakout_speed);
    char brk_speed_unit = 'M';
    uint_t post_fix = (media_info->media_interface_prefix
            * media_info->media_interface_lane_count)
                       / ( (far_end < 1) ? 1 : far_end);

    if (brk_speed >= 1000) {
        brk_speed_unit = 'G';
        brk_speed /= 1000;
    }

    if (far_end > 1){
        snprintf (far_end_str, sizeof(far_end_str), "%ux(", far_end);
    } else if (media_info->media_interface_prefix > 1){

        post_fix /= media_info->media_interface_prefix;
        snprintf (prefix_str, sizeof(prefix_str), "%d",
              media_info->media_interface_prefix);

        snprintf (far_end_str, sizeof(far_end_str), "%ux", media_info->media_interface_prefix);
        brk_speed /= media_info->media_interface_prefix;
    }

    if (post_fix > 1) {
        snprintf (postfix_str, sizeof(postfix_str), "%d", post_fix);
    }

    if (media_interface_qualifier_str[0] == '\0'){
        snprintf (media_if_str, sizeof(media_if_str),"%c%s%s%s", separator, prefix_str,
              media_interface_str, postfix_str);
    } else {
        snprintf (media_if_str, sizeof(media_if_str),"%c%s%s%s%c%s", separator, prefix_str,
               media_interface_str, postfix_str, separator, media_interface_qualifier_str);
    }

    if (brk_speed > 0){
        snprintf (speed_str, sizeof(speed_str)," %s%d%cBASE%s", far_end_str
              , brk_speed
              , brk_speed_unit
              , (media_info->capability_list[0].phy_mode
                               == BASE_IF_PHY_MODE_TYPE_FC) ? fc_str : "");
    }

    if (media_info->cable_length_cm > 0) {
        snprintf(length_str, sizeof(length_str),"%c%.1fM",
                separator, ((float)(media_info->cable_length_cm))/100.0);
        snprintf(output, len,"%s%s%s%s%s",
                        trans_str, speed_str, media_if_str,(far_end > 1) ? ")" :"", length_str);
    }else {
        snprintf(output, len,"%s%s%s%s",
                        trans_str, speed_str, media_if_str, (far_end > 1) ? ")" :"");
    }
}

/* After creating display string, add the adapter info if it exists */
static void pas_media_append_adapter_info(dn_pas_basic_media_info_t* media_info, char* output, size_t len)
{
    const char* qsa_str = pas_media_get_qsa_string_from_enum(media_info->qsa_adapter_type);
     if (qsa_str != NULL){
        if (strlen(output) + strlen(qsa_str) <  len){
            strcat(output, qsa_str);
         } else {
            PAS_ERR ("Unable to append QSA info %s to media display string %s ; because name too long for buffer",
                qsa_str, output);
         }
     }
 }


/* Get the cable length in cm */
/* Need enhancement */


uint_t pas_media_get_cable_length_cm (phy_media_tbl_t *mtbl)
{
    return mtbl->res_data->length_cable * 100;
}

static void pas_media_resolve_autoneg (phy_media_tbl_t *mtbl)
{
    BASE_IF_SUPPORTED_AUTONEG_t an = BASE_IF_SUPPORTED_AUTONEG_OFF_SUPPORTED;

    /** Will add code for fetching from config **/

    /* For fixed ports */
    if (mtbl->res_data->port_type == PLATFORM_PORT_TYPE_FIXED) {
        mtbl->media_info.default_autoneg =
                BASE_IF_SUPPORTED_AUTONEG_ON_SUPPORTED;
        return;
    }

    /* For pluggable ports*/
    switch (mtbl->media_info.cable_type){
        case PLATFORM_MEDIA_CABLE_TYPE_DAC:
            switch (mtbl->media_info.capability_list[0].breakout_speed){
                case BASE_IF_SPEED_10GIGE:
                    an = BASE_IF_SUPPORTED_AUTONEG_ON_SUPPORTED;
                    break;
                default:
                    an = BASE_IF_SUPPORTED_AUTONEG_OFF_SUPPORTED;
                    break;
            }
            break;

        case PLATFORM_MEDIA_CABLE_TYPE_FIBER:
            switch (mtbl->media_info.transceiver_type){
                case PLATFORM_MEDIA_CATEGORY_SFP:
                    switch (mtbl->media_info.media_interface) {
                        case PLATFORM_MEDIA_INTERFACE_SX:
                        case PLATFORM_MEDIA_INTERFACE_LX:
                            an = BASE_IF_SUPPORTED_AUTONEG_ON_SUPPORTED;
                            break;
                        default:
                            an = BASE_IF_SUPPORTED_AUTONEG_OFF_SUPPORTED;
                            break;
                    }
                    break;
                default:
                    an = BASE_IF_SUPPORTED_AUTONEG_OFF_SUPPORTED;
                    break;
            }
            break;

        default:
            an = BASE_IF_SUPPORTED_AUTONEG_OFF_SUPPORTED;
            break;
    }
    mtbl->media_info.default_autoneg = an;
}

/* Derive the basic media info from info in EEPROM and maps */

static bool pas_media_populate_basic_media_info(
                dn_pas_basic_media_info_t* media_info, sdi_media_connector_t raw_conn)
{
    PLATFORM_MEDIA_CONNECTOR_TYPE_t
            conn1 = PLATFORM_MEDIA_CONNECTOR_TYPE_UNKNOWN,
            conn2 = PLATFORM_MEDIA_CONNECTOR_TYPE_UNKNOWN,
            conn_from_eeprom = PLATFORM_MEDIA_CONNECTOR_TYPE_UNKNOWN;
    PLATFORM_MEDIA_CABLE_TYPE_t cab1 = PLATFORM_MEDIA_CABLE_TYPE_UNKNOWN,
                                cab2 = PLATFORM_MEDIA_CABLE_TYPE_UNKNOWN;
    bool no_qual = true;

    if (media_info->ext_spec_code_25g_dac == PLATFORM_EXT_SPEC_COMPLIANCE_CODE_UNKNOWN){
        media_info->ext_spec_code_25g_dac = PLATFORM_EXT_SPEC_COMPLIANCE_CODE_NOT_APPLICABLE;
    }

    conn_from_eeprom = pas_media_get_media_connector_enum(raw_conn);
    conn1 = pas_media_get_media_interface_connector_type_expected(
            media_info->media_interface);
    cab1 = pas_media_get_media_interface_cable_type_expected(
            media_info->media_interface);

    /* Handle qualifer if needed */
    if ((media_info->media_interface_qualifier
                  != PLATFORM_MEDIA_INTERFACE_QUALIFIER_NO_QUALIFIER)
    && (media_info->media_interface_qualifier
                  != PLATFORM_MEDIA_INTERFACE_QUALIFIER_UNKNOWN)) {
        conn2 = pas_media_get_media_interface_qualifier_connector_type_expected(
                  media_info->media_interface_qualifier);
        if (conn2 != PLATFORM_MEDIA_CONNECTOR_TYPE_UNKNOWN){
            cab2 = pas_media_get_media_interface_qualifier_cable_type_expected(
                  media_info->media_interface_qualifier);
        }
        no_qual = false;
    }
    media_info->cable_type = (cab2 == PLATFORM_MEDIA_CABLE_TYPE_UNKNOWN) ? cab1: cab2;

    media_info->connector_type = ((conn_from_eeprom
                == PLATFORM_MEDIA_CONNECTOR_TYPE_UNKNOWN)
                ? conn_from_eeprom
                : ((no_qual) ? conn1 : conn2));

    /* If no entry exists, use entry from media interface connector */
    if (!pas_media_is_media_connector_separable(media_info->connector_type,
                &(media_info->connector_separable))){
        if (!pas_media_is_media_connector_separable(conn2,
                &(media_info->connector_separable))){
                    if (!pas_media_is_media_connector_separable(conn1,
                           &(media_info->connector_separable))){
                        return false;
            }
        }
    }
    return true;
}


/* Function to get media properties including display string, connector, cable etc */

bool pas_media_get_media_properties(phy_media_tbl_t *mtbl)
{
    dn_pas_basic_media_info_t* media_info = &(mtbl->media_info);
    media_info->transceiver_type      = 0;
    media_info->connector_type        = PLATFORM_MEDIA_CONNECTOR_TYPE_UNKNOWN;
    media_info->cable_type            = PLATFORM_MEDIA_CABLE_TYPE_UNKNOWN;
    media_info->media_interface       = PLATFORM_MEDIA_INTERFACE_UNKNOWN;
    media_info->media_interface_qualifier = PLATFORM_MEDIA_INTERFACE_QUALIFIER_UNKNOWN;
    media_info->media_interface_lane_count = 1;
    media_info->media_interface_prefix = 1;
    media_info->cable_length_cm      = 0;
    media_info->connector_separable  = false;
    media_info->default_autoneg = BASE_IF_SUPPORTED_AUTONEG_OFF_SUPPORTED;
    media_info->default_fec = 0;
    media_info->qsa_adapter_type = PLATFORM_QSA_ADAPTER_UNKNOWN;
    media_info->ext_spec_code_25g_dac = PLATFORM_EXT_SPEC_COMPLIANCE_CODE_UNKNOWN;
    media_info->qsa28_expected = false;

    char temp_override_str[sizeof(media_info->media_name)] = {0};

    pas_media_disc_cb_t disc_cb      = NULL;
    bool ret                         = false;

    media_info->transceiver_type      = mtbl->res_data->category;
    sdi_media_connector_t raw_conn    = mtbl->res_data->connector;

    /* should be changed to derive capability manually */
    memcpy(&(media_info->capability_list[0]),
             &(mtbl->res_data->media_capabilities[0]),
                    sizeof(media_capability_t));

    /* Get the callback to handle given trans type */
    disc_cb = pas_media_get_disc_cb_from_trans_type(
                            media_info->transceiver_type);
    safestrncpy(media_info->display_string, PAS_MEDIA_UNKNOWN_MEDIA, sizeof(media_info->display_string));
    safestrncpy(media_info->transceiver_type_string,
                   PAS_MEDIA_UNKNOWN_MEDIA_CATEGORY, sizeof(media_info->transceiver_type_string));

    if (disc_cb == NULL) {
        PAS_ERR("FATAL: No media discovery implementation for media on port %u",
                           mtbl->fp_port);

        return false;
    }
    safestrncpy(media_info->transceiver_type_string,
                  (mtbl->res_data->port_type == PLATFORM_PORT_TYPE_FIXED)
                  ? "RJ45"
                  : pas_media_get_transceiver_type_display_string(media_info->transceiver_type),
                      sizeof(media_info->transceiver_type_string));
    ret = disc_cb (mtbl, media_info);

    /* to do capability stuff */

    if (!pas_media_populate_basic_media_info(media_info, raw_conn)){
        //ERROR
        ret &= false;
    }
    media_info->cable_length_cm = (media_info->connector_separable) ? 0 : pas_media_get_cable_length_cm(mtbl);

    /* This forces anything that either has no breakout or is 1x1, to be 1x1*/
    if (pas_media_map_get_breakout_far_end_val(media_info->capability_list[0].breakout_mode) <= 1) {
        media_info->capability_list[0].breakout_mode = BASE_CMN_BREAKOUT_TYPE_BREAKOUT_1X1;
        mtbl->res_data->media_capabilities[0].breakout_mode = BASE_CMN_BREAKOUT_TYPE_BREAKOUT_1X1;
    }

    media_info->qsa_adapter_type =  mtbl->res_data->qsa_adapter_type;

    media_info->qsa28_expected = ((media_info->qsa_adapter_type == PLATFORM_QSA_ADAPTER_QSA)
                                 && (media_info->capability_list[0].media_speed == BASE_IF_SPEED_25GIGE));

    /* Naming logic: Media name is used between applications to quickly refer to media
       Media names follow a strict convention that does should never change
       However the display string (which is what the user sees) may be slightly or completely different form the media name used by apps.
       In most cases, display string only differs by the use of hyphens instead of spaces.
       However in some cases, a very different string is needed, so overrides are used */

    /* Media name uses spaces*/
    pas_media_construct_base_display_string(media_info, ' ', media_info->media_name, sizeof(media_info->media_name));

    /* Get media override */
    safestrncpy(temp_override_str, pas_media_get_media_name_override_from_derived_name(media_info->media_name)
                                                    , sizeof (temp_override_str));

    /*If no override exists, PAS_MEDIA_UNKNOWN_MEDIA is returned. In this case, create the display string using hyphens. Else copy the override string  */
    if (strncmp(temp_override_str, PAS_MEDIA_UNKNOWN_MEDIA, strlen(PAS_MEDIA_UNKNOWN_MEDIA)) == 0){
        pas_media_construct_base_display_string(media_info, '-', media_info->display_string, sizeof(media_info->display_string));
    } else {
        safestrncpy(media_info->display_string, temp_override_str, sizeof(media_info->display_string));
     }

    /* Append extra info to display string */
    pas_media_append_adapter_info(media_info, media_info->display_string, sizeof(media_info->display_string));

    pas_media_resolve_autoneg(mtbl);

    if ((media_info->display_string)[0] == '\0') {
        ret &= false;
    }

    /* If legacy type is not know, try to derive from name */
    if (mtbl->res_data->type == PLATFORM_MEDIA_TYPE_AR_POPTICS_UNKNOWN) {
        mtbl->res_data->type = pas_media_get_enum_from_new_media_name (media_info->display_string);
    }

    /* Update res_data struct */
    memcpy(&(mtbl->res_data->media_capabilities[0]),
               &(media_info->capability_list[0]),
                    sizeof(media_capability_t));
    mtbl->res_data->capability = media_info->capability_list[0].media_speed;

    return ret;
}
