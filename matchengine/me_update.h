/*
 * Description: 
 *     History: yang@haipo.me, 2017/03/18, create
 */

# ifndef _ME_UPDATE_H_
# define _ME_UPDATE_H_

int init_update(void);
int update_user_balance(bool real, uint32_t user_id, const char *asset, const char *business, uint64_t business_id, mpd_t *change, json_t *detail);
int transfer_user_balance(bool real, uint32_t user_id1, uint32_t user_id2, const char *asset, const char *business, uint64_t business_id, mpd_t *amount, json_t *detail);
int moving_user_balance(bool real, uint32_t src_uid, uint32_t dst_uid, uint32_t src_part_id, uint32_t dst_part_id,const char *asset, mpd_t *amount, const char *business, uint64_t business_id, json_t *detail);
int exchange_user_balance(bool real, uint32_t user_id1, uint32_t user_id2, const char *fromAsset, mpd_t *amount1, const char *toAsset, mpd_t *amount2, uint64_t business_id, json_t *detail);
int cash_user_balance(bool real, uint32_t user_id1, const char *fromAsset, mpd_t *amount1, const char *toAsset, mpd_t *amount2, uint64_t business_id, json_t *detail);
int freeze_user_balance(bool real, uint32_t user_id, const char *asset, mpd_t *amount, uint64_t business_id, json_t *detail, bool is_freeze);
# endif

