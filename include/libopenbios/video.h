
#ifdef CONFIG_VGA_WIDTH
#define VGA_DEFAULT_WIDTH	CONFIG_VGA_WIDTH
#else
#define VGA_DEFAULT_WIDTH	800
#endif

#ifdef CONFIG_VGA_HEIGHT
#define VGA_DEFAULT_HEIGHT	CONFIG_VGA_HEIGHT
#else
#define VGA_DEFAULT_HEIGHT	600
#endif

#ifdef CONFIG_VGA_DEPTH
#define VGA_DEFAULT_DEPTH	CONFIG_VGA_DEPTH
#else
#define VGA_DEFAULT_DEPTH	8
#endif

#define VGA_DEFAULT_LINEBYTES	(VGA_DEFAULT_WIDTH*((VGA_DEFAULT_DEPTH+7)/8))

void setup_video(phys_addr_t phys, ucell virt);
void init_video(void);
unsigned long video_get_color(int col_ind);
void video_set_color(int ind, unsigned long color);
int video_get_res(int *w, int *h);
void video_mask_blit(void);
void video_invert_rect(void);
void video_fill_rect(void);

extern struct video_info {
    int has_video;

    volatile phys_addr_t mphys;
    volatile ucell *mvirt;
    volatile ucell *rb, *w, *h, *depth;

    volatile ucell *pal;    /* 256 elements */
} video;

#define VIDEO_DICT_VALUE(x)  (*(ucell *)(x))
