#include "ble_pub.h"
#include "hal_ble.h"
#include <stddef.h>
#include <stdint.h>
#include <aos/aos.h>
#include "ali_crypto.h"

#define KEY_LEN 16

ais_adv_init_t ais_adv_init_info;

extern struct bd_addr common_default_bdaddr;

int ble_disconnect_flag = 0;

ais_err_t ble_stack_init(ais_bt_init_t *ais_init)
{
    memcpy((uint8_t *)&ais_bt_init_info,(uint8_t *)ais_init,sizeof(ais_bt_init_t));
    //memset((uint8_t *)&ais_bt_init_info,0,sizeof(ais_bt_init_t));
    //ble_dut_start();
    ble_activate(NULL);
    return AIS_ERR_SUCCESS;
}

ais_err_t ble_stack_deinit()
{
    memset((uint8_t *)&ais_bt_init_info,0,sizeof(ais_bt_init_t));

    ble_send_msg(BLE_MSG_EXIT);

    bk_printf("ble_stack_deinit\r\n");
    
    return AIS_ERR_SUCCESS;
}

ais_err_t ble_send_notification(uint8_t *p_data, uint16_t length)
{
    ais_err_t status = AIS_ERR_SUCCESS;
	
    status = feb3_send_fed8_ntf_value(length,p_data,0xff);
	
    return status;
}

ais_err_t ble_send_indication(uint8_t *p_data, uint16_t length)
{
    ais_err_t status = AIS_ERR_SUCCESS;

    status = feb3_send_fed6_ind_value(length,p_data,0xff);
	
    return status;
}

void ble_disconnect(uint8_t reason)
{
    bk_printf("ble_disconnect reason = 0x%x\r\n",reason);

    appm_disconnect(reason);
}

ais_err_t ble_advertising_start(ais_adv_init_t *adv)
{
    ais_err_t status = AIS_ERR_SUCCESS;
    
    memcpy((uint8_t *)&ais_adv_init_info,(uint8_t *)adv,sizeof(ais_adv_init_t));
    status = appm_start_advertising(adv);

    return status;
}

ais_err_t ble_advertising_stop()
{
    ais_err_t status = AIS_ERR_SUCCESS;
    
    status = appm_stop_advertising();

    return status;
}

ais_err_t ble_get_mac(uint8_t *mac)
{
    ais_err_t status = AIS_ERR_SUCCESS;
    
    memcpy(mac, &common_default_bdaddr, AIS_BT_MAC_LEN);

    return status;
}

void *ais_aes128_init(const uint8_t *key, const uint8_t *iv)
{
    ali_crypto_result result;
    void             *aes_ctx;
    size_t            aes_ctx_size, alloc_siz;
    uint8_t          *p;

    result = breeze_aes_get_ctx_size(AES_CBC, &aes_ctx_size);
    if (result != ALI_CRYPTO_SUCCESS) {
        LOGE("aos_awss", "get ctx size fail(%08x)", result);
        return NULL;
    }

    alloc_siz = aes_ctx_size + KEY_LEN * 2 + sizeof(bool);
    aes_ctx   = aos_malloc(alloc_siz);
    if (aes_ctx == NULL) {
        LOGE("aos_awss", "kmalloc(%d) fail", (int)aes_ctx_size);
        return NULL;
    }
    memset(aes_ctx, 0, alloc_siz);

    p = (uint8_t *)aes_ctx + aes_ctx_size;
    memcpy(p, key, KEY_LEN);
    p += KEY_LEN;
    memcpy(p, iv, KEY_LEN);

    return aes_ctx;
}

int ais_aes128_destroy(void *aes)
{
    if (aes) {
        aos_free(aes);
    }

    return 0;
}

static int platform_aes128_encrypt_decrypt(void *aes_ctx, const void *src,
                                           size_t siz, void *dst, bool is_enc)
{
    ali_crypto_result result;
    size_t            dlen, in_len = siz, ctx_siz;
    uint8_t          *p, *key, *iv;
    if (aes_ctx == NULL) {
        LOGE("aos_awss", "platform_aes128_encrypt_decrypt aes_ctx is NULL");
        return -1;
    }
    result = breeze_aes_get_ctx_size(AES_CBC, &ctx_siz);
    if (result != ALI_CRYPTO_SUCCESS) {
        LOGE("aos_awss", "get ctx size fail(%08x)", result);
        return 0;
    }

    p   = (uint8_t *)aes_ctx + ctx_siz;
    key = p;
    p += KEY_LEN;
    iv = p;

    in_len <<= 4;
    dlen = in_len;

    result = breeze_aes_init(AES_CBC, is_enc, key, NULL, KEY_LEN, iv, aes_ctx);
    if (result != ALI_CRYPTO_SUCCESS) {
        LOGE("aos_awss", "breeze_aes_init fail(%08x)", result);
        return 0;
    }

    result = breeze_aes_finish(src, in_len, dst, &dlen, SYM_NOPAD, aes_ctx);
    if (result != ALI_CRYPTO_SUCCESS) {
        LOGE("aos_awss", "aes_finish fail(%08x)", result);
        return -1;
    }

    return 0;
}

int ais_aes128_cbc_encrypt(void *aes, const void *src, size_t block_num,
                           void *dst)
{
    return platform_aes128_encrypt_decrypt(aes, src, block_num, dst, true);
}

int ais_aes128_cbc_decrypt(void *aes, const void *src, size_t block_num,
                           void *dst)
{
    return platform_aes128_encrypt_decrypt(aes, src, block_num, dst, false);
}
