/*
 * Description: 
 *     History: yang@haipo.me, 2017/03/18, create
 */

# include "me_config.h"
# include "me_update.h"
# include "me_balance.h"
# include "me_history.h"
# include "me_message.h"

static dict_t *dict_update;
static nw_timer timer;

struct update_key {
    uint32_t    user_id;
    uint32_t    user_id2;
    char        asset[ASSET_NAME_MAX_LEN + 1];
    char        business[BUSINESS_NAME_MAX_LEN + 1];
    uint64_t    business_id;
};

struct update_val {
    double      create_time;
};

static uint32_t update_dict_hash_function(const void *key)
{
    return dict_generic_hash_function(key, sizeof(struct update_key));
}

static int update_dict_key_compare(const void *key1, const void *key2)
{
    return memcmp(key1, key2, sizeof(struct update_key));
}

static void *update_dict_key_dup(const void *key)
{
    struct update_key *obj = malloc(sizeof(struct update_key));
    memcpy(obj, key, sizeof(struct update_key));
    return obj;
}

static void update_dict_key_free(void *key)
{
    free(key);
}

static void *update_dict_val_dup(const void *val)
{
    struct update_val*obj = malloc(sizeof(struct update_val));
    memcpy(obj, val, sizeof(struct update_val));
    return obj;
}

static void update_dict_val_free(void *val)
{
    free(val);
}

static void on_timer(nw_timer *t, void *privdata)
{
    double now = current_timestamp();
    dict_iterator *iter = dict_get_iterator(dict_update);
    dict_entry *entry;
    while ((entry = dict_next(iter)) != NULL) {
        struct update_val *val = entry->val;
        if (val->create_time < (now - 86400)) {
            dict_delete(dict_update, entry->key);
        }
    }
    dict_release_iterator(iter);
}

int init_update(void)
{
    dict_types type;
    memset(&type, 0, sizeof(type));
    type.hash_function  = update_dict_hash_function;
    type.key_compare    = update_dict_key_compare;
    type.key_dup        = update_dict_key_dup;
    type.key_destructor = update_dict_key_free;
    type.val_dup        = update_dict_val_dup;
    type.val_destructor = update_dict_val_free;

    dict_update = dict_create(&type, 64);
    if (dict_update == NULL)
        return -__LINE__;

    nw_timer_set(&timer, 60, true, on_timer, NULL);
    nw_timer_start(&timer);

    return 0;
}

int update_user_balance(bool real, uint32_t user_id, const char *asset, const char *business, uint64_t business_id, mpd_t *change, json_t *detail)
{
    struct update_key key = {.user_id2 = 0};
    key.user_id = user_id;
    strncpy(key.asset, asset, sizeof(key.asset));
    strncpy(key.business, business, sizeof(key.business));
    key.business_id = business_id;

    dict_entry *entry = dict_find(dict_update, &key);
    if (entry) {
        return -1;
    }

    mpd_t *result;
    mpd_t *abs_change = mpd_new(&mpd_ctx);
    mpd_abs(abs_change, change, &mpd_ctx);
    if (mpd_cmp(change, mpd_zero, &mpd_ctx) >= 0) {
        result = balance_add(user_id, BALANCE_TYPE_AVAILABLE, asset, abs_change);
    } else {
        result = balance_sub(user_id, BALANCE_TYPE_AVAILABLE, asset, abs_change);
    }
    mpd_del(abs_change);
    if (result == NULL)
        return -2;

    struct update_val val = { .create_time = current_timestamp() };
    dict_add(dict_update, &key, &val);

    if (real) {
        double now = current_timestamp();
        json_object_set_new(detail, "id", json_integer(business_id));
        char *detail_str = json_dumps(detail, 0);
        append_user_balance_history(now, user_id, asset, business, change, detail_str);
        free(detail_str);
        push_balance_message(now, user_id, asset, business, change);
    }

    return 0;
}

int freeze_user_balance(bool real, uint32_t user_id, const char *asset, mpd_t *amount, uint64_t business_id, json_t *detail, bool is_freeze) {
    const char *business = is_freeze ? "freeze" : "unfreeze";

    struct update_key key = {.user_id = user_id, .user_id2 = 0, .business_id = business_id};
    strncpy(key.asset, asset, sizeof(key.asset));
    strncpy(key.business, business, sizeof(key.business));

    // 是否重复发送请求
    dict_entry *entry = dict_find(dict_update, &key);
    if (entry)
        return -1;

    mpd_t *r = NULL;
    if (is_freeze) {
        r = balance_manual_freeze(user_id, asset, amount);
    } else {
        r = balance_manual_unfreeze(user_id, asset, amount);
    }

    if (r == NULL)
        return -2;

    struct update_val val = { .create_time = current_timestamp() };
    dict_add(dict_update, &key, &val);

    if (real) {
        double now = current_timestamp();
        json_object_set_new(detail, "id", json_integer(business_id));
        json_object_set_new_mpd(detail, "a", amount);
        char *detail_str = json_dumps(detail, 0);
        append_user_balance_history(now, user_id, asset, business, mpd_zero, detail_str);
        free(detail_str);
//        push_balance_message(now, user_id, asset, business, mpd_zero); // TODO
    }

    return 0;
}

//moving balance from one user to another
int moving_user_balance(bool real, uint32_t src_uid, uint32_t dst_uid,
        uint32_t src_part_id, uint32_t dst_part_id,
         const char *asset, mpd_t *amount,
         const char *business, uint64_t business_id, json_t *detail)
{
    struct update_key key = {.user_id = src_uid, .user_id2 = dst_uid};
    strncpy(key.asset, asset, sizeof(key.asset));
    strncpy(key.business, business, sizeof(key.business));
    key.business_id = business_id;

    // 是否重复发送请求
    dict_entry *entry = dict_find(dict_update, &key);
    if (entry)
        return -1;

    //amount must > 0
    if (!amount || mpd_cmp(amount, mpd_zero, &mpd_ctx) <= 0)
        return -2;

    //检查源User对应分区余额
    mpd_t *src_balance = balance_get(src_uid, src_part_id, asset);
    if (!src_balance || mpd_cmp(src_balance, amount, &mpd_ctx) < 0)
        return -3;

    // 后面的操作不会失败，当作已经成功
    struct update_val val = { .create_time = current_timestamp() };
    dict_add(dict_update, &key, &val);

    // 调整src_uid, dst_uid的asset的数量
    balance_sub(src_uid, src_part_id, asset, amount);
    balance_add(dst_uid, dst_part_id, asset, amount);

    if (real) {
        double now = current_timestamp();
        json_object_set_new(detail, "id", json_integer(business_id));
         json_object_set_new(detail, "sp", json_integer(src_part_id));
          json_object_set_new(detail, "dp", json_integer(dst_part_id));
        char *detail_str = json_dumps(detail, 0);

        // asset, dst_uid: amount是正数
        append_user_balance_history(now, dst_uid, asset, business, amount, detail_str);
        push_balance_message(now, dst_uid, asset, business, amount);

        // asset, src_uid: amount是负数
        mpd_set_negative(amount);
        append_user_balance_history(now, src_uid, asset, business, amount, detail_str);
        push_balance_message(now, src_uid, asset, business, amount);

        free(detail_str);
    }
    return 0;
}


// amount > 0, amount2 > 0
int transfer_user_balance(bool real, uint32_t user_id1, uint32_t user_id2, const char *asset, const char *business, uint64_t business_id, mpd_t *amount, json_t *detail)
{
    //const char *business = "transfer";

    struct update_key key = {.user_id2 = user_id2};
    key.user_id = user_id1;
    strncpy(key.asset, asset, sizeof(key.asset));
    strncpy(key.business, business, sizeof(key.business));
    key.business_id = business_id;

    // 是否重复发送请求
    dict_entry *entry = dict_find(dict_update, &key);
    if (entry)
        return -1;

    // user_id1 -> user_id2 asset amount 是否足够
    mpd_t *user1_balance = balance_get(user_id1, BALANCE_TYPE_AVAILABLE, asset);
    if (!user1_balance || mpd_cmp(user1_balance, amount, &mpd_ctx) < 0)
        return -2;

    // 后面的操作不会失败，当作已经成功
    struct update_val val = { .create_time = current_timestamp() };
    dict_add(dict_update, &key, &val);

    // 调整user_id1, user_id2的asset1的数量
    balance_sub(user_id1, BALANCE_TYPE_AVAILABLE, asset, amount);
    balance_add(user_id2, BALANCE_TYPE_AVAILABLE, asset, amount);

    if (real) {
        double now = current_timestamp();
        json_object_set_new(detail, "id", json_integer(business_id));
        char *detail_str = json_dumps(detail, 0);

        // asset1, user_id2是正数
        append_user_balance_history(now, user_id2, asset, business, amount, detail_str);
        push_balance_message(now, user_id2, asset, business, amount);

        // asset1, user_id1是负数
        mpd_set_negative(amount);
        append_user_balance_history(now, user_id1, asset, business, amount, detail_str);
        push_balance_message(now, user_id1, asset, business, amount);

        free(detail_str);
    }
    return 0;
}

// exchange 0:user1ID 1:user2ID 2:asset1 3:amount1 4:asset2 5:amount2 6:businessID 7:detail
int exchange_user_balance(bool real, uint32_t user_id1, uint32_t user_id2, const char *fromAsset, mpd_t *amount_a, const char *toAsset, mpd_t *amount_b, uint64_t business_id, json_t *detail)
{
    const char *business = "exchange";
    struct update_key key = {.user_id2 = user_id2};
    key.user_id = user_id1;
    strncpy(key.asset, fromAsset, sizeof(key.asset));
    strncpy(key.asset + strlen(fromAsset), toAsset, sizeof(key.asset) - strlen(fromAsset));
    strncpy(key.business, business, sizeof(key.business));
    key.business_id = business_id;

    // 是否重复发送请求
    dict_entry *entry = dict_find(dict_update, &key);
    if (entry)
        return -1;

    // user_id1 -> user_id2, amount是否足够
    mpd_t *user1_balance1 = balance_get(user_id1, BALANCE_TYPE_AVAILABLE, fromAsset);
    if (!user1_balance1 || mpd_cmp(user1_balance1, amount_a, &mpd_ctx) < 0)
        return -2;

    // user_id2 -> user_id1, amount2是否足够
    mpd_t *user2_balance2 = balance_get(user_id2, BALANCE_TYPE_AVAILABLE, toAsset);
    if (!user2_balance2 || mpd_cmp(user2_balance2, amount_b, &mpd_ctx) < 0)
        return -2;

    // 后面的操作不会失败，当作已经成功
    struct update_val val = { .create_time = current_timestamp() };
    dict_add(dict_update, &key, &val);

    // 调整user_id1, user_id2的asset1的数量. user_id1 -> user_id2, fromAsset, amount
    balance_sub(user_id1, BALANCE_TYPE_AVAILABLE, fromAsset, amount_a);
    balance_add(user_id2, BALANCE_TYPE_AVAILABLE, fromAsset, amount_a);

    // 调整user_id1, user_id2的asset2的数量. user_id2 -> user_id1, toAsset, amount2
    balance_sub(user_id2, BALANCE_TYPE_AVAILABLE, toAsset, amount_b);
    balance_add(user_id1, BALANCE_TYPE_AVAILABLE, toAsset, amount_b);

    if (real) {
        double now = current_timestamp();
        json_object_set_new(detail, "id", json_integer(business_id));
        char *detail_str = json_dumps(detail, 0);

        // asset1, user_id2 加正数. user_id1 -> user_id2, fromAsset, amount
        append_user_balance_history(now, user_id2, fromAsset, business, amount_a, detail_str);
        push_balance_message(now, user_id2, fromAsset, business, amount_a);

        // asset1, user_id1 加负数. user_id1 -> user_id2, fromAsset, amount
        mpd_set_negative(amount_a);
        append_user_balance_history(now, user_id1, fromAsset, business, amount_a, detail_str);
        push_balance_message(now, user_id1, fromAsset, business, amount_a);

        // asset2, user_id1 加正数. user_id2 -> user_id1, toAsset, amount2
        append_user_balance_history(now, user_id1, toAsset, business, amount_b, detail_str);
        push_balance_message(now, user_id1, toAsset, business, amount_b);

        // asset2, user_id2 加负数. user_id2 -> user_id1, toAsset, amount2
        mpd_set_negative(amount_b);
        append_user_balance_history(now, user_id2, toAsset, business, amount_b, detail_str);
        push_balance_message(now, user_id2, toAsset, business, amount_b);

        free(detail_str);
    }

    return 0;
}

// cash 0:user1ID 1:asset1 2:amount1 3:asset2 4:amount2 5:businessID 6:detail
int cash_user_balance(bool real, uint32_t user_id1, const char *fromAsset, mpd_t *amount_a, const char *toAsset, mpd_t *amount_b, uint64_t business_id, json_t *detail)
{
    const char *business = "cash";
    struct update_key key = {.user_id2 = user_id1};
    key.user_id = user_id1;
    strncpy(key.asset, fromAsset, sizeof(key.asset));
    strncpy(key.asset + strlen(fromAsset), toAsset, sizeof(key.asset) - strlen(fromAsset));
    strncpy(key.business, business, sizeof(key.business));
    key.business_id = business_id;

    // 是否重复发送请求
    dict_entry *entry = dict_find(dict_update, &key);
    if (entry)
        return -1;

    // user_id1, fromAsset -> toAsset, amount是否足够
    mpd_t *user1_balance1 = balance_get(user_id1, BALANCE_TYPE_AVAILABLE, fromAsset);
    if (!user1_balance1 || mpd_cmp(user1_balance1, amount_a, &mpd_ctx) < 0)
        return -2;

    // 后面的操作不会失败，当作已经成功
    struct update_val val = { .create_time = current_timestamp() };
    dict_add(dict_update, &key, &val);

    // 调整user_id1的fromAsset和toAsset的数量. fromAsset -> toAsset, amount_a->amount_b
    balance_sub(user_id1, BALANCE_TYPE_AVAILABLE, fromAsset, amount_a);
    balance_add(user_id1, BALANCE_TYPE_AVAILABLE, toAsset, amount_b);

    if (real) {
        double now = current_timestamp();
        json_object_set_new(detail, "id", json_integer(business_id));
        char *detail_str = json_dumps(detail, 0);

        // toAsset, user_id1 加正数. fromAsset -> toAsset, toAsset, amount
        append_user_balance_history(now, user_id1, toAsset, business, amount_a, detail_str);
        push_balance_message(now, user_id1, toAsset, business, amount_a);

        // fromAsset, user_id1 加负数. fromAsset -> toAsset, fromAsset, amount
        mpd_set_negative(amount_b);
        append_user_balance_history(now, user_id1, fromAsset, business, amount_b, detail_str);
        push_balance_message(now, user_id1, fromAsset, business, amount_b);

        free(detail_str);
    }

    return 0;
}