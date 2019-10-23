/***************************************************************************//**
 * @file
 * @brief Event handling and application code for Empty NCP Host application example
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

/* standard library headers */
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <mbedtls/md5.h>

/* BG stack headers */
#include "bg_types.h"
#include "gecko_bglib.h"

/* Own header */
#include "app.h"
//#include "dump.h"
#include "cic.h"

// App booted flag
static bool appBooted = false;
static struct {
  uint8 channel_map;
} config = {
	    .channel_map = 7,
};

void parse_address(const char *fmt,bd_addr *address) {
  char buf[3];
  int octet;
  for(uint8 i = 0; i < 6; i++) {
    memcpy(buf,&fmt[3*i],2);
    buf[2] = 0;
    sscanf(buf,"%02x",&octet);
    address->addr[5-i] = octet;
  }
}

const char *getAppOptions(void) {
  return "m:";
}

void appOption(int option, const char *arg) {
  int i;
  switch(option) {
  case 'm':
    sscanf(arg,"%d",&i);
    if((i & 7)&&!(i >> 3)) {
      config.channel_map = i;
    } else {
      fprintf(stderr,"channel map must be 0 - 7\n");
      exit(1);
    }
    break;
  default:
    fprintf(stderr,"Unhandled option '-%c'\n",option);
    exit(1);
  }
}

void appInit(void) {
}

void dump_flags(uint8 flags) {
  printf("Flags: %02x\n",flags); //return;
  printf("  LE Limited Discoverable Mode: %d\n",flags&1);
  printf("  LE General Discoverable Mode: %d\n",(flags>>1)&1);
  printf("  BR/EDR Not Supported: %d\n",(flags>>2)&1);
  printf("  Simultaneous LE and BR/EDR to Same Device Capable (Controller).: %d\n",(flags>>3)&1);
  printf("  Simultaneous LE and BR/EDR to Same Device Capable (Host).: %d\n",(flags>>4)&1);
  if(flags >> 5) printf("  Reserved bits set: %02x",flags);
}

void dump_incomplete_list_of_16bit_services(uint8 len,uint8*data) {
  if(len & 1) {
    printf("length of 16-bit service UUIDs is odd!\n");
    return;
  }
  printf("Incomplete List of 16-bit Service Class UUIDs:");
  for(int i = 0; i < len; i += 2) {
    printf(" %02x%02x",data[i+1],data[i]);
  }
  printf("\n");
}

void dump_complete_list_of_16bit_services(uint8 len,uint8*data) {
  if(len & 1) {
    printf("length of 16-bit service UUIDs is odd!\n");
    return;
  }
  printf("Complete List of 16-bit Service Class UUIDs:");
  for(int i = 0; i < len; i += 2) {
    printf(" %02x%02x",data[i+1],data[i]);
  }
  printf("\n");
}

void dump_complete_local_name(uint8 len, uint8* data) {
  char str[len+1];
  memcpy(str,data,len);
  str[len]= 0;
  printf("Complete Local Name: '%s'\n",str);
}

void dump_service_data_16(uint8 len, uint8* data) {
  printf("Service Data - 16-bit UUID: %02x%02x:",data[1],data[0]);
  for(int i = 2; i < len; i++) {
    printf(" %02x",data[i]);
  }
  printf("\n");
}

void dump_service_data_128(uint8 len, uint8* data) {
  printf("Service Data - 128-bit UUID: ");
  for(int i = 0; i < 16; i++) printf("%02x",data[15-i]);
  printf(":");
  for(int i = 16; i < len; i++) {
    printf(" %02x",data[i]);
  }
  printf("\n");
}

void dump_manufacturer_specific_data(uint8 len,uint8 *data) {
  uint16 id = *(uint16_t*)data;
  printf("Manufacturer Specific Data:\n");
  printf("  Manufacturer Identifier: %04x %s\n",id,get_cic(id));
  printf("  Data:");
  for (int i = 2; i < len; i++) printf(" %02x",data[i]);
  printf("\n");
}

int dump_element(uint8 type,uint8 len, uint8*data) {
  switch(type) {
  case 0x01:
    dump_flags(data[0]);
    break;
  case 0x02:
    dump_incomplete_list_of_16bit_services(len,data);
    break;
  case 0x03:
    dump_complete_list_of_16bit_services(len,data);
    break;
  case 0x09:
    dump_complete_local_name(len,data);
    break;
  case 0x0a:
    printf("TX Power Level: %d dBm\n",((int8*)data)[0]);
    break;
  case 0x16:
    dump_service_data_16(len,data);
    break;
  case 0x21:
    dump_service_data_128(len,data);
    break;
  case 0xff:
    dump_manufacturer_specific_data(len,data);
    break;
  default:
    printf("Unhandled advertising element: type: %02x, len: %d\n",type,len);
    return 1;
  }
  return 0;
}

int dump_advertisement(uint8 len, uint8*data) {
  uint8 i = 0;
  while(i < len) {
    uint8 elen = data[i++];
    uint8 type = data[i];
    if(dump_element(type,elen-1,&data[i+1])) return 1;
    i += elen;
  }
  return 0;
}

void dump_address(bd_addr address,uint8 type,int8 rssi) {
  const char *ts = "*illegal address type*";
  switch(type) {
  case 0: ts = "public"; break;
  case 1: ts = "random"; break;
  case 255: ts = "anonymous"; break;
  }
  printf("Address: ");
  for(int i = 0; i < 6; i++) printf("%s%02x",(i)?":":"",address.addr[5-i]);
  printf(" %d dBm (%s)\n",rssi,ts);
}

void dump_packet_type(uint8 packet_type) {
  char *cs;
  switch(packet_type & 7) {
  case 0: cs = "Connectable scannable undirected advertising"; break;
  case 1: cs = "Connectable undirected advertising"; break;
  case 2: cs = "Scannable undirected advertising"; break;
  case 3: cs = "Non-connectable non-scannable undirected advertising"; break;
  case 4: cs = "Scan Response"; break;
  }
  printf("Packet type: %s\n",cs);
}

uint8 unique[512][16],md5sum[16];
uint16 ucount = 0;


/***********************************************************************************************//**
 *  \brief  Event handler function.
 *  \param[in] evt Event pointer.
 **************************************************************************************************/
void appHandleEvents(struct gecko_cmd_packet *evt)
{
  mbedtls_md5_context ctx;
  if (NULL == evt) {
    return;
  }

  // Do not handle any events until system is booted up properly.
  if ((BGLIB_MSG_ID(evt->header) != gecko_evt_system_boot_id)
      && !appBooted) {
#if defined(DEBUG)
    printf("Event: 0x%04x\n", BGLIB_MSG_ID(evt->header));
#endif
    usleep(50000);
    return;
  }

  /* Handle events */
#ifdef DUMP
  dump_event(evt);
#endif
  switch (BGLIB_MSG_ID(evt->header)) {
    case gecko_evt_system_boot_id:
      appBooted = true;
      mbedtls_md5_init(&ctx);
      gecko_cmd_system_linklayer_configure(3,1,&config.channel_map);
      gecko_cmd_le_gap_set_discovery_extended_scan_response(1);
      gecko_cmd_le_gap_set_discovery_timing(le_gap_phy_1m,1000,1000);
      gecko_cmd_le_gap_set_discovery_type(le_gap_phy_1m,1);
      gecko_cmd_le_gap_start_discovery(le_gap_phy_1m,le_gap_discover_observation);
      break;
  case gecko_evt_le_gap_extended_scan_response_id:
#define ED evt->data.evt_le_gap_extended_scan_response
    mbedtls_md5_starts_ret(&ctx);
    mbedtls_md5_update_ret(&ctx,&ED.address.addr[0],6);
    mbedtls_md5_update_ret(&ctx,&ED.data.data[0],ED.data.len);
    mbedtls_md5_finish_ret(&ctx,md5sum);
    for(int i = 0; i < ucount; i++)
      if(0 == memcmp(unique[i],md5sum,16)) return;
    memcpy(unique[ucount++],md5sum,16);
    dump_address(ED.address,ED.address_type,ED.rssi);
    dump_packet_type(ED.packet_type);
    if(dump_advertisement(ED.data.len,ED.data.data)) {
      dump_event(evt);
      exit(1);
    };
    printf("\n");
    break;
#undef ED
    default:
      break;
  }
}
