
#include <stdio.h>
#include <syslog.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <sys/mman.h>

#include <bcm_host.h>

int process() {
    DISPMANX_DISPLAY_HANDLE_T display;
    DISPMANX_MODEINFO_T display_info;
    DISPMANX_RESOURCE_HANDLE_T screen_resource;
    VC_IMAGE_TRANSFORM_T transform;
    uint32_t image_prt;
    VC_RECT_T rect1;
    int ret;
    int fbfd = 0;
    char *fbp = 0;

    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;


    bcm_host_init();

    display = vc_dispmanx_display_open(0);
    if (!display) {
        syslog(LOG_ERR, "Unable to open primary display");
        return -1;
    }
    ret = vc_dispmanx_display_get_info(display, &display_info);
    if (ret) {
        syslog(LOG_ERR, "Unable to get primary display information");
        return -1;
    }
    syslog(LOG_INFO, "Primary display is %d x %d", display_info.width, display_info.height);


    fbfd = open("/dev/fb1", O_RDWR);
    if (fbfd == -1) {
        syslog(LOG_ERR, "Unable to open secondary display");
        return -1;
    }
    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo)) {
        syslog(LOG_ERR, "Unable to get secondary display information");
        return -1;
    }
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo)) {
        syslog(LOG_ERR, "Unable to get secondary display information");
        return -1;
    }

    syslog(LOG_INFO, "Second display is %d x %d %dbps\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);

    /***********************************************************
     * Name: vc_dispmanx_resource_create
     *
     * Arguments:
     *       VC_IMAGE_TYPE_T type
     *       uint32_t width
     *       uint32_t height
     *
     * Description: Create a new resource (in Videocore memory)
     *
     * Returns: resource handle
     *
     ***********************************************************/

    int resourceWidth = vinfo.xres / 2;
    int resourceHeight = vinfo.yres;


    screen_resource = vc_dispmanx_resource_create(VC_IMAGE_RGB565, resourceWidth, resourceHeight, &image_prt);
    if (!screen_resource) {
        syslog(LOG_ERR, "Unable to create screen buffer");
        close(fbfd);
        vc_dispmanx_display_close(display);
        return -1;
    }

    fbp = (char*) mmap(0, finfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
    if (fbp <= 0) {
        syslog(LOG_ERR, "Unable to create memory mapping");
        close(fbfd);
        ret = vc_dispmanx_resource_delete(screen_resource);
        vc_dispmanx_display_close(display);
        return -1;
    }


    // vinfo = SECONDARY fb width and height

    int width = vinfo.xres / 2;
    int height = vinfo.yres;
    // width = 100;
    // height = 400;

    /***********************************************************
     * Name: vc_dispmanx_rect_set
     *
     * Arguments:
     *       VC_RECT_T *rect
     *       uint32_t x_offset
     *       uint32_t y_offset
     *       uint32_t width
     *       uint32_t height
     *
     * Description: Fills in the fields of the supplied VC_RECT_T structure
     *
     * Returns: 0 or failure
     *
     ***********************************************************/

    vc_dispmanx_rect_set(&rect1, 0, 0, width, height);

    // fbfd = /dev/fb1 (secondary display)
    // fbp = pointer to pixels
    // display = PRIMARY display

    while (1) {

        /***********************************************************
         * Name: vc_dispmanx_snapshot
         *
         * Arguments:
         *       DISPMANX_DISPLAY_HANDLE_T display
         *       DISPMANX_RESOURCE_HANDLE_T snapshot_resource
         *       DISPMANX_TRANSFORM_T transform
         *
         * Description: Take a snapshot of a display in its current state
         *
         * Returns:
         *
         ***********************************************************/

        // primary display, buffer, transformations(!)

        ret = vc_dispmanx_snapshot(display, screen_resource, 0);

        /***********************************************************
         * Name: vc_dispmanx_resource_read_data
         *
         * Arguments:
         *       DISPMANX_RESOURCE_HANDLE_T res
         *       const VC_RECT_T * rect
         *       void * src_address
         *       int src_pitch
         *
         * Description: Copy the bitmap data from VideoCore memory
         *
         * Returns: 0 or failure
         *
         ***********************************************************/

        vc_dispmanx_resource_read_data(screen_resource, &rect1, fbp, (width / 2.0) * vinfo.bits_per_pixel / 8);
        usleep(25 * 1000);
    }

    munmap(fbp, finfo.smem_len);
    close(fbfd);
    ret = vc_dispmanx_resource_delete(screen_resource);
    vc_dispmanx_display_close(display);
}

int main(int argc, char **argv) {
    setlogmask(LOG_UPTO(LOG_DEBUG));
    openlog("fbcp", LOG_NDELAY | LOG_PID, LOG_USER);

    return process();
}
