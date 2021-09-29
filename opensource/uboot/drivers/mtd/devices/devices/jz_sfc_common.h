#ifndef SFC_COMMON_H
#define SFC_COMMON_H


void sfc_start(struct sfc *sfc);
void sfc_flush_fifo(struct sfc *sfc);
void sfc_mode(struct sfc *sfc, int channel, int value);
void sfc_set_addr_length(struct sfc *sfc, int channel, unsigned int value);
void sfc_cmd_enble(struct sfc *sfc, int channel, unsigned int value);
void sfc_write_cmd(struct sfc *sfc, int channel, unsigned int value);
void sfc_set_cmd_length(struct sfc *sfc, unsigned int value);
void sfc_dev_data_dummy_bits(struct sfc *sfc, int channel, unsigned int value);
void sfc_dev_pollen(struct sfc *sfc, int channel, unsigned int value);
void sfc_dev_sta_exp(struct sfc *sfc, unsigned int value);
void sfc_dev_sta_msk(struct sfc *sfc, unsigned int value);
void sfc_clear_all_intc(struct sfc *sfc);
void sfc_enable_all_intc(struct sfc *sfc);
void sfc_set_data_length(struct sfc *sfc, int value);
unsigned int sfc_get_sta_rt(struct sfc *sfc);

void dump_sfc_reg(struct sfc *sfc);

void sfc_message_init(struct sfc_message *m);
void sfc_message_add_tail(struct sfc_transfer *t, struct sfc_message *m);
void sfc_transfer_del(struct sfc_transfer *t);
int sfc_sync(struct sfc *sfc, struct sfc_message *message);
struct sfc *sfc_res_init(unsigned int sfc_rate);

int set_flash_timing(struct sfc *sfc, unsigned int t_hold, unsigned int t_setup, unsigned int t_shslrd, unsigned int t_shslwr);

int sfc_nor_get_special_ops(struct sfc_flash *flash);

#endif
