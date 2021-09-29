#include <config.h>
#include <common.h>
DECLARE_GLOBAL_DATA_PTR;
struct ddr_registers *g_ddr_param = 0;
struct param_info
{
	unsigned int magic_id;
	unsigned int size;
	unsigned int data;
};
void *find_param(unsigned int magic_id)
{
	struct param_info *p = (struct param_info *)CONFIG_SPL_GINFO_BASE;
	do {
		if(magic_id == p->magic_id)
		{
			break;
		}
		p = (struct param_info *)((char *)p + p->size + 8);

	}while(p->magic_id != 0);
	return (void*)&p->data;
}
void burner_param_info(void)
{
	gd->arch.gi = find_param(('B' << 24) | ('D' << 16) | ('I' << 8) | ('F' << 0));
	g_ddr_param = find_param(('D' << 24) | ('D' << 16) | ('R' << 8) | 0);
}
