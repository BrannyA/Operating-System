#include "mac.h"
#include "irq.h"

#define NUM_DMA_DESC 48
queue_t recv_block_queue;
uint32_t recv_flag[256] = {0};
uint32_t ch_flag;
uint32_t mac_cnt = 0;

int received_num = 0;  // packages haved received

desc_t Tx_desc[256];  // Send
desc_t Rx_desc[256];  // Receive

uint32_t reg_read_32(uint32_t addr) {
    return *((uint32_t *) addr);
}

uint32_t read_register(uint32_t base, uint32_t offset) {
    uint32_t addr = base + offset;
    uint32_t data;

    data = *(volatile uint32_t *) addr;
    return data;
}

void reg_write_32(uint32_t addr, uint32_t data) {
    *((uint32_t *) addr) = data;
}

static void gmac_get_mac_addr(uint8_t *mac_addr) {
    uint32_t addr;

    addr = read_register(GMAC_BASE_ADDR, GmacAddr0Low);
    mac_addr[0] = (addr >> 0) & 0x000000FF;
    mac_addr[1] = (addr >> 8) & 0x000000FF;
    mac_addr[2] = (addr >> 16) & 0x000000FF;
    mac_addr[3] = (addr >> 24) & 0x000000FF;

    addr = read_register(GMAC_BASE_ADDR, GmacAddr0High);
    mac_addr[4] = (addr >> 0) & 0x000000FF;
    mac_addr[5] = (addr >> 8) & 0x000000FF;
}

void print_tx_dscrb(mac_t *mac) {
    uint32_t i;
    printf("send buffer mac->saddr=0x%x ", mac->saddr);
    printf("mac->saddr_phy=0x%x ", mac->saddr_phy);
    printf("send discrb mac->td_phy=0x%x\n", mac->td_phy);
#if 0
    desc_t *send=mac->td;
    for(i=0;i<mac->pnum;i++)
    {
        printf("send[%d].tdes0=0x%x ",i,send[i].tdes0);
        printf("send[%d].tdes1=0x%x ",i,send[i].tdes1);
        printf("send[%d].tdes2=0x%x ",i,send[i].tdes2);
        printf("send[%d].tdes3=0x%x ",i,send[i].tdes3);
    }
#endif
}

void print_rx_dscrb(mac_t *mac) {
    uint32_t i;
    printf("ieve buffer add mac->daddr=0x%x ", mac->daddr);
    printf("mac->daddr_phy=0x%x ", mac->daddr_phy);
    printf("recieve discrb add mac->rd_phy=0x%x\n", mac->rd_phy);
    desc_t *recieve = (desc_t *) mac->rd;
#if 0
    for(i=0;i<mac->pnum;i++)
    {
        printf("recieve[%d].tdes0=0x%x ",i,recieve[i].tdes0);
        printf("recieve[%d].tdes1=0x%x ",i,recieve[i].tdes1);
        printf("recieve[%d].tdes2=0x%x ",i,recieve[i].tdes2);
        printf("recieve[%d].tdes3=0x%x\n",i,recieve[i].tdes3);
    }
#endif
}


// void check_recv(mac_t *test_mac) {
//     desc_t *receive = (desc_t *) test_mac->rd;
//     int i, j, print_location = 3, wrong = 0;
//     for (i = 0; i < 256; i++) {
//         if (!((*((uint32_t *) recv_flag[i])) & 0x8000f8cf)) {
//             cnt++;
//         }
//         (*((uint32_t *) recv_flag[i])) = DescOwnByDma;
//         reg_write_32(DMA_BASE_ADDR + 0x8, 1);
//     }
// }

static uint32_t printf_recv_buffer(uint32_t recv_buffer) {
}

void mac_irq_handle(void) {
    // check whether new recv packet is arriving
    if (!queue_is_empty(&recv_block_queue))
        do_unblock_one(&recv_block_queue);
    clear_interrupt();
}

void irq_enable(int IRQn) {
    // INT1_EN register's 3rd bit, Open or Close mac interrupt
    reg_write_32(0xbfd0105c, 0x8);
}

//Clears all the pending interrupts.
//If the Dma status register is read then all the interrupts gets cleared
void clear_interrupt() {
    uint32_t data;
    data = reg_read_32(0xbfe11000 + DmaStatus);
    reg_write_32(0xbfe11000 + DmaStatus, data);
}

void mac_recv_handle(mac_t *test_mac) {
    // only use this function in task 3.
    int i, j, pos;
    desc_t *r_desc = (desc_t *) test_mac->rd;
    pos = 2;
    // if(time == 1)   pos = 3;
    // else pos = 9;
    while (received_num < PNUM) {
        if (!(r_desc[received_num].tdes0 >> 31)) {
            uint32_t *recv_addr = (uint32_t * )(test_mac->daddr + PSIZE * 4 * received_num);
            sys_move_cursor(1, pos + 1);
            printf("%d recv buff, rdes0 = 0x%x:\n", received_num, r_desc[received_num].tdes0);

            if (recv_addr[0] != 0x5e0001 && recv_addr[0] != 0x7f5e0001 && recv_addr[0] != 0xffffffff &&
                recv_addr[0] != 0x3333 && received_num % 64 == 0)
                for (j = 0; j < PNUM; j++)
                    printf("%x ", recv_addr[j]);

            printf("\n");
            received_num++;
        } else {
            sys_move_cursor(1, pos);
            printf(">>[RECV TASK] waiting for the %dth package.   \n", received_num);
            sys_wait_recv_package();
        }
    }
    sys_move_cursor(1, 1);
    printf("> [RECV TASK] Totally Receive %d Packages!             \n", received_num);
}

void set_sram_ctr() {
    *((volatile unsigned int *) 0xbfd00420) = 0x8000;
}

static void s_reset(mac_t *mac) //reset mac regs
{
    uint32_t time = 1000000;
    reg_write_32(mac->dma_addr, 0x01);

    while ((reg_read_32(mac->dma_addr) & 0x01)) {
        reg_write_32(mac->dma_addr, 0x01);
        while (time) {
            time--;
        }
    };
}

void disable_interrupt_all(mac_t *mac) {
    reg_write_32(mac->dma_addr + DmaInterrupt, DmaIntDisable);
    return;
}

void set_mac_addr(mac_t *mac) {
    uint32_t data;
    uint8_t MacAddr[6] = {0x00, 0x55, 0x7b, 0xb5, 0x7d, 0xf7};
    uint32_t MacHigh = 0x40, MacLow = 0x44;
    data = (MacAddr[5] << 8) | MacAddr[4];
    reg_write_32(mac->mac_addr + MacHigh, data);
    data = (MacAddr[3] << 24) | (MacAddr[2] << 16) | (MacAddr[1] << 8) | MacAddr[0];
    reg_write_32(mac->mac_addr + MacLow, data);
}

uint32_t do_net_recv(uint32_t rd, uint32_t rd_phy, uint32_t daddr) {
    desc_t *r_desc = (desc_t *) rd;
    reg_write_32(DMA_BASE_ADDR + 0xc, rd_phy);
    reg_write_32(GMAC_BASE_ADDR, reg_read_32(GMAC_BASE_ADDR) | 0x4);
    reg_write_32(DMA_BASE_ADDR + 0x18, reg_read_32(DMA_BASE_ADDR + 0x18) | 0x02200002); // start tx, rx
    reg_write_32(DMA_BASE_ADDR + 0x1c, 0x10001 | (1 << 6));
    int i;
    desc_t *receive = (desc_t *) rd;
    for (i = 0; i < 256; i++) {
        receive[i].tdes0 = DescOwnByDma;
    }
    for (i = 0; i < 256; i++)
        reg_write_32(DMA_BASE_ADDR + 0x8, 1);
    return 0;
    //task1
    // while(r_desc[0].tdes0 >> 31){
    //     vt100_move_cursor(1, 11);
    //     printk("Waiting receive package...");
    // }
    // for(i = 0; i < PNUM; i++){
    //     if( !(r_desc[i].tdes0 >> 31) & !(r_desc[i].tdes0 >> 30)){
    //         vt100_move_cursor(1, 5);
    //         printk("%d recv buff, rdes0 = 0x%x:                 \n", count, r_desc[i].tdes0);
    //         for(j = 0; j < PSIZE; j++)
    //             printk("%x ", recv_addr[PSIZE * count + j]);
    //         printk("\n");
    //         count ++;
    //     }
    // }
    // printk("Totally recv %d valid packages!    \n", count);
    // return (int)(count == 0);
}


void do_net_send(uint32_t td, uint32_t td_phy) {
    int i;
    desc_t *s_desc = (desc_t *) td;
    reg_write_32(DMA_BASE_ADDR + 0x10, td_phy);

    // MAC rx/tx enable
    reg_write_32(GMAC_BASE_ADDR, reg_read_32(GMAC_BASE_ADDR) | 0x8);                    // enable MAC-TX
    reg_write_32(DMA_BASE_ADDR + 0x18, reg_read_32(DMA_BASE_ADDR + 0x18) | 0x02202000); //0x02202002); // start tx, rx
    reg_write_32(DMA_BASE_ADDR + 0x1c, 0x10001 | (1 << 6));

    desc_t *send = (desc_t *) td;
    for (i = 0; i < 64; i++) {
        send[i].tdes0 = DescOwnByDma;
    }
    //for(i = 0; i < 64; i++)
    reg_write_32(DMA_BASE_ADDR + 0x4, 1);
}

void do_init_mac(void) {
    mac_t test_mac;
    uint32_t i;

    test_mac.mac_addr = 0xbfe10000;
    test_mac.dma_addr = 0xbfe11000;

    test_mac.psize = PSIZE * 4; // 64bytes
    test_mac.pnum = PNUM;       // pnum

    set_sram_ctr();
    s_reset(&test_mac);
    disable_interrupt_all(&test_mac);
    set_mac_addr(&test_mac);

    reg_write_32(INT1_CLR, 0xffffffff);
    reg_write_32(INT1_POL, 0xffffffff);
    reg_write_32(INT1_EDGE, 0);
}

void do_wait_recv_package(void) {
    do_block(&recv_block_queue);
}
